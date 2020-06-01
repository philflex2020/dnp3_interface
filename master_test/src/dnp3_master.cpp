
/* this is the outline of a master dnp3 controller
* the system can communicate with a number outstations
* The confg file will determine the address info on each outstation.
* each outsation will have a number of data points.
* a feature of dnp3 is the need to povide a shadow  stack to hold the user defined data points
* iniitally a copy of the modbus stuff
*/
/*
 * dnp3_master.cpp
 *
 *  Created on: Sep 28, 2020
 *      Author: jcalcagni - modbus
 *              pwilshire - dnp3 version
 */

#include <stdio.h>
#include <string>
#include <string.h>
#include <map>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <signal.h>

#include <arpa/inet.h>
#include <stdint.h>
#include <cjson/cJSON.h>
#include <fims/libfims.h>

#include <iostream>
#include <asiopal/UTCTimeSource.h> 
#include <opendnp3/LogLevels.h> 
#include <asiodnp3/ConsoleLogger.h> 
#include <asiodnp3/DNP3Manager.h> 
//#include <asiodnp3/DefaultMasterApplication.h> 
//#include <asiodnp3/PrintingChannelListener.h> 
//#include <asiodnp3/PrintingCommandCallback.h> 
#include <asiopal/ChannelRetry.h>


#include "fpsSOEHandler.h"
#include "fpsMasterApplication.h"
#include "fpsLogger.h"
#include "fpsChannelListener.h"
#include "fpsCommandCallback.h"
#include "dnp3_utils.h"

using namespace std; 
using namespace openpal; 
using namespace asiopal; 
using namespace asiodnp3; 
using namespace opendnp3; 
#define MICROSECOND_TO_MILLISECOND 1000
#define NANOSECOND_TO_MILLISECOND  1000000
#define MAX_FIMS_CONNECT 5

volatile bool running = true;
int ttick = 0;  // timeout ticks for replies to initial gets

void signal_handler (int sig)
{
    running = false;
    FPS_ERROR_PRINT("signal of type %d caught.\n", sig);
    signal(sig, SIG_DFL);
}

DNP3Manager* setupDNP3Manager(sysCfg* ourDB)
{
    auto manager = new DNP3Manager(1, fpsLogger::Create(ourDB));
    return manager;
}

// I think we can have several channels under one manager
std::shared_ptr<IChannel> setupDNP3channel(DNP3Manager* manager, const char* cname, sysCfg* ourDB, const char* addr, int port)
{

     // Specify what log levels to use. NORMAL is warning and above
    // You can add all the comms logging by uncommenting below
    const uint32_t FILTERS = levels::NORMAL ;//| levels::ALL_APP_COMMS;
    // This is the main point of interaction with the stack
    // Connect via a TCPClient socket to a outstation
    // repeat for each outstation

    // TODO setup our own PrintingChannelListener
    std::shared_ptr<IChannel> channel = manager->AddTCPClient(cname 
                                        , FILTERS 
                                        , ChannelRetry::Default() 
                                        , {IPEndpoint(addr, port)}
                                        ,"0.0.0.0" 
                                        , fpsChannelListener::Create(ourDB));
    // The master config object for a master. The default are
    // useable, but understanding the options are important.
    return channel;
}

 //auto master = setupDNP3master (channel, "master", &sys_cfg , sys_cfg.local_address, sys_cfg.remote_address /*RemoteAddr*/);
std::shared_ptr<IMaster> setupDNP3master (std::shared_ptr<IChannel> channel, const char* mname, sysCfg* ourDB )
{
    MasterStackConfig stackConfig;
    // you can override application layer settings for the master here
    // in this example, we've change the application layer timeout to 2 seconds
    stackConfig.master.responseTimeout = TimeDuration::Seconds(2);
    stackConfig.master.disableUnsolOnStartup = true;
    // You can override the default link layer settings here
    // in this example we've changed the default link layer addressing
    stackConfig.link.LocalAddr = ourDB->local_address;//localAddr; // 1
    stackConfig.link.RemoteAddr = ourDB->remote_address;//remoteAddr; //10;
    // Create a new master on a previously declared port, with a
    // name, log level, command acceptor, and config info. This
    // returns a thread-safe interface used for sending commands.
    
    auto master = channel->AddMaster("master", // id for logging
                                     fpsSOEHandler::Create(ourDB), // callback for data processing  this generates the pub elements when we get data
                                     fpsMasterApplication::Create(ourDB), // master application instance this manages the collection of al the pub elements 
                                     stackConfig // stack configuration
                                    );

    // do an integrity poll (Class 3/2/1/0) once per minute
    //auto integrityScan = master->AddClassScan(ClassField::AllClasses(), TimeDuration::Minutes(1));
    // TODO we need a way to demand this via FIMS
    // ourDB->frequency
    auto integrityScan = master->AddClassScan(ClassField::AllClasses(), TimeDuration::Milliseconds(ourDB->frequency));
    // do a Class 1 exception poll every 5 seconds
    //auto exceptionScan = master->AddClassScan(ClassField(ClassField::CLASS_1), TimeDuration::Seconds(60));
    
    //auto binscan = master->AddAllObjectsScan(GroupVariationID(1,1),
    //                                                               TimeDuration::Seconds(5));
    //auto objscan = master->AddAllObjectsScan(GroupVariationID(30,1),
    //                                                               TimeDuration::Seconds(5));
    //auto objscan2 = master->AddAllObjectsScan(GroupVariationID(30,5),
    //                                                               TimeDuration::Seconds(10));
    // Enable the master. This will start communications.
    master->Enable();
    // bool channelCommsLoggingEnabled = true;
    // bool masterCommsLoggingEnabled = true;
    bool channelCommsLoggingEnabled = false;
    bool masterCommsLoggingEnabled = false;

    auto levels = channelCommsLoggingEnabled ? levels::ALL_COMMS : levels::NORMAL;
    channel->SetLogFilters(levels);
    levels = masterCommsLoggingEnabled ? levels::ALL_COMMS : levels::NORMAL;
    master->SetLogFilters(levels);
    // other master controls are :
    //
    return master;
}

void addVarToCommands (CommandSet & commands, std::pair<DbVar*,int>dbp)
{
    DbVar* db = dbp.first;
    //int flag = dbp.second;

    switch (db->type) 
    {
        case Type_Crob:
        {
            commands.Add<ControlRelayOutputBlock>({WithIndex(ControlRelayOutputBlock(ControlCodeFromType(db->crob)), db->idx)});
            break;
        }
        case AnIn16:
        {
            commands.Add<AnalogOutputInt16>({WithIndex(AnalogOutputInt16(db->anInt16), db->idx)});
            break;
        }
        case AnIn32:
        {
            commands.Add<AnalogOutputInt32>({WithIndex(AnalogOutputInt32(db->anInt32), db->idx)});
            break;
        }
        case AnF32:
        {
            commands.Add<AnalogOutputFloat32>({WithIndex(AnalogOutputFloat32(db->anF32), db->idx)});
            break;
        }
        default:
            break;
    }
}


int main(int argc, char *argv[])
{
    fims* p_fims;
    p_fims = new fims();
    
    //char* uri;
    //bool publish_only[3] = {false,false,false};
    bool running = true;
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    //int fd_max = 0;
    //int rc = 0;
    //int server_socket = -1;
    int fims_connect = 0;
    //fd_set all_connections;
    //FD_ZERO(&all_connections);
    sysCfg sys_cfg;
    
    //memset(&sys_cfg, 0, sizeof(sysCfg));
    //datalog data[Num_Register_Types];
    //memset(data, 0, sizeof(datalog) * Num_Register_Types);

    FPS_DEBUG_PRINT("Reading config file and starting setup.\n");
    cJSON* config = get_config_json(argc, argv);
    if(config == NULL)
    {
        FPS_ERROR_PRINT("Error reading config file\n");
        return 1;
    }
    if(!parse_system(config, &sys_cfg)) 
    {
        FPS_ERROR_PRINT("Error reading system from config file.\n");
        cJSON_Delete(config);
        return 1;
    }
    if(!parse_variables(config, &sys_cfg)) 
    {
        FPS_ERROR_PRINT("Error reading variabled from config file.\n");
        cJSON_Delete(config);
        return 1;
    }

    cJSON_Delete(config);

    sys_cfg.setupReadb((const char*)"master");
    sys_cfg.showDbMap();
    sys_cfg.showUris();

    const char *sub_array[]={
        (const char *)"/interfaces",
        (const char*)"/fooey",
        NULL
        };
    const char **subs = NULL;
    bool *bpubs = NULL;
    int num = getSysUris(&sys_cfg, "master", subs, bpubs, sub_array, 2);
    if(num < 0)
    {
        FPS_ERROR_PRINT("Failed to create subs array.\n");
        return 1;
    }

    sys_cfg.p_fims = p_fims;// = new fims();
    if (p_fims == NULL)
    {
        FPS_ERROR_PRINT("Failed to allocate connection to FIMS server.\n");
        //rc = 1;
        return 1;//goto cleanup;
    }
    // could alternatively fims connect using a stored name for the server
    while(fims_connect < MAX_FIMS_CONNECT && p_fims->Connect(sys_cfg.id) == false)
    {
        fims_connect++;
        sleep(1);
    }

    if(fims_connect >= MAX_FIMS_CONNECT)
    {
        FPS_ERROR_PRINT("Failed to establish connection to FIMS server.\n");
        //rc = 1;
        return 1;
        //goto cleanup;
    }

    FPS_DEBUG_PRINT("Map configured: Initializing data.\n");
    //if(p_fims->Subscribe((const char**)sub_array, 3, (bool *)publish_only) == false)
    if(p_fims->Subscribe((const char**)subs, num, (bool *)bpubs) == false)
    {
        FPS_ERROR_PRINT("Subscription failed.\n");
        p_fims->Close();
        return 1;
    }

    free((void *)subs);

    auto manager = setupDNP3Manager(&sys_cfg);
    if (!manager){
        FPS_ERROR_PRINT("Error in setupDNP3Manager.\n");
        return 1;
    }
    // now we use data from the config file
    //std::shared_ptr<IChannel> 
    //auto channel = setupDNP3channel(manager, "tcpclient1", "127.0.0.1", 20001);
    auto channel = setupDNP3channel(manager, sys_cfg.id, &sys_cfg, sys_cfg.ip_address, sys_cfg.port);
    if (!channel){
        FPS_ERROR_PRINT("Error in setupDNP3channel.\n");
        delete manager;
        return 1;
    }
        //std::shared_ptr<IChannel> 
    auto master = setupDNP3master (channel, "master", &sys_cfg );
    if (!master){
        FPS_ERROR_PRINT("Error in setupDNP3master.\n");
        delete manager;
        return 1;
    }
    //we cant do this
    //auto master2 = setupDNP3master (channel2, "master2", ourDB , 2 /*localAddr*/ , 10 /*RemoteAddr*/);

    FPS_DEBUG_PRINT("DNP3 Setup complete: Entering main loop.\n");
    
    // send out intial gets
    // set max ticks
    sys_cfg.getUris("master");
    // set all values to inval  done at the start
    // start time to complete gets
    // TODO set for all the getURI responses as todo
    // done only get outstation vars 

    while(running && p_fims->Connected())
    {
        // once a second
        fims_message* msg = p_fims->Receive_Timeout(1000000);
        if(msg == NULL)
        { 
            // TODO check for all the getURI resposes
            ttick++;
            bool ok = sys_cfg.checkUris("master");
            if(ok == false)
            {
                FPS_DEBUG_PRINT("Timeout tick2 %d\n", ttick);

                if (ttick > MAX_SETUP_TICKS)
                {
                    // just quit here
                    FPS_DEBUG_PRINT("QUITTING TIME Timeout tick %d\n", ttick);
                }
            }
        }
        else //if(msg != NULL)
        {
            dbs_type dbs; // collect all the parsed vars here
            cJSON* cjb = parseBody(dbs, &sys_cfg, msg, "master");
    
            if(dbs.size() > 0)
            {
                CommandSet commands;
                cJSON*cj = NULL;                
                if(msg->replyto != NULL)
                    cj = cJSON_CreateObject();

                while (!dbs.empty())
                {
                    std::pair<DbVar*,int>dbp = dbs.back();
                    addVarToCommands(commands, dbp);
                    if(cj) addVarToCj(cj, dbp);
                    dbs.pop_back();
                }

                if(cj)
                {
                    const char* reply = cJSON_PrintUnformatted(cj);
                    cJSON_Delete(cj);
                    cj = NULL;
                    // TODO check that SET is the correct thing to do in MODBUS_CLIENT
                    // probably OK since this is a reply
                    p_fims->Send("set", msg->replyto, NULL, reply);
                    free((void* )reply);
                }

                FPS_DEBUG_PRINT("      *****Running Direct Outputs \n");
                master->DirectOperate(std::move(commands), fpsCommandCallback::Get());
            }

            if (cjb != NULL)
            {
                cJSON_Delete(cjb);
            }
            p_fims->free_message(msg);
        }
    }

    //cleanup:
    if (manager) delete manager;
    // sys_cfg should clean itself up
 
    //if(sys_cfg.eth_dev       != NULL) free(sys_cfg.eth_dev);
    //if(sys_cfg.ip_address    != NULL) free(sys_cfg.ip_address);
    //if(sys_cfg.name          != NULL) free(sys_cfg.name);
    //if(sys_cfg.serial_device != NULL) free(sys_cfg.serial_device);
    //if(sys_cfg.mb != NULL)             modbus_free(sys_cfg.mb);
    // for(int fd = 0; fd < fd_max; fd++)
    //     if(FD_ISSET(fd, &all_connections))
    //         close(fd);
    return 0;
}


