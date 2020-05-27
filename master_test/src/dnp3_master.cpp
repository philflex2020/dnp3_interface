
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
#include <cjson/cJSON.h>
#include <fims/libfims.h>

#include <iostream>
#include <asiopal/UTCTimeSource.h> 
#include <opendnp3/LogLevels.h> 
#include <asiodnp3/ConsoleLogger.h> 
#include <asiodnp3/DNP3Manager.h> 
#include <asiodnp3/DefaultMasterApplication.h> 
#include <asiodnp3/PrintingChannelListener.h> 
#include <asiodnp3/PrintingCommandCallback.h> 
#include <asiopal/ChannelRetry.h>


#include "fpsSOEHandler.h"
#include "newMasterApplication.h"

#include "fpsLogger.h"

#include "fpsChannelListener.h"


#include "dnp3_utils.h"

using namespace std; 
using namespace openpal; 
using namespace asiopal; 
using namespace asiodnp3; 
using namespace opendnp3; 
#define MICROSECOND_TO_MILLISECOND 1000
#define NANOSECOND_TO_MILLISECOND  1000000
#define MAX_FIMS_CONNECT 5
#define MAX_SETUP_TICKS  50

volatile bool running = true;
int ttick = 0;  // timeout ticks for replies to initial gets

void signal_handler (int sig)
{
    running = false;
    FPS_ERROR_PRINT("signal of type %d caught.\n", sig);
    signal(sig, SIG_DFL);
}

// // Deprecated
// bool process_fims_message(fims_message* msg, server_data* server_map)
// {

//     char* uri = msg->uri;
//     // uri was sent to this processes URI so it needs to handle it
//     if(strncmp(uri, server_map->base_uri, strlen(server_map->base_uri)) == 0)
//     {
//         if(strcmp("set", msg->method) == 0)
//         {
//             uri = msg->pfrags[2];
//             if(msg->nfrags > 2 && strncmp("reply", uri, strlen("reply")) == 0)
//             {
//                 uri = msg->pfrags[3] - 1;
//                 uri_map::iterator uri_it = server_map->uri_to_register.find(uri);
//                 if(uri_it == server_map->uri_to_register.end())
//                 {
//                     // failed to find in map
//                     FPS_ERROR_PRINT( "Fims reply for unrequested uri: %s.\n", uri);
//                     return false;
//                 }
//                 body_map* body = uri_it->second;
//                 cJSON* body_obj = cJSON_Parse(msg->body);
//                 //check for valid json
//                 if(body_obj == NULL)
//                 {
//                     FPS_ERROR_PRINT(" Received invalid json object for set %s. \"%s\"\n", uri, msg->body);
//                     return false;
//                 }
//                 for(body_map::iterator body_it = body->begin(); body_it != body->end(); ++body_it)
//                 {
//                     cJSON* cur_obj = cJSON_GetObjectItem(body_obj, body_it->first);
//                     if(cur_obj == NULL)
//                     {
//                         FPS_ERROR_PRINT("Failed to find %s in %s.\n", body_it->first, uri);
//                         return false;
//                     }
//                     update_register_value(server_map->mb_mapping, body_it->second.first, body_it->second.second, cur_obj);
//                 }
//             }
//             else
//             {
//                 if(msg->replyto != NULL)
//                     server_map->p_fims->Send("set", msg->replyto, NULL, "Error: Method not implemented for that URI.");
//                 return true;
//             }
//         }
//         else if(strcmp("get", msg->method) == 0)
//         {
//             //don't know when we would receive a get
//             if(msg->replyto != NULL)
//                 server_map->p_fims->Send("set", msg->replyto, NULL, "Error: Method not implemented for that URI.");
//             return true;
//         }
//         else
//         {
//             if(msg->replyto != NULL)
//                 server_map->p_fims->Send("set", msg->replyto, NULL, "Error: Method not implemented for that URI.");
//             return true;
//         }
//     }
//     else // Check to see if this is a publish we care about
//     {
//         if(strcmp("pub", msg->method) == 0)
//         {
//             uri_map::iterator uri_it = server_map->uri_to_register.find(msg->uri);
//             if(uri_it == server_map->uri_to_register.end())
//             {
//                 //failed to find uri in devices we care about.
//                 FPS_ERROR_PRINT("Received pub not in uri list: %s.\n", msg->uri);
//                 return false;
//             }
//             cJSON* body_obj = cJSON_Parse(msg->body);
//             if(body_obj == NULL || body_obj->child == NULL)
//             {
//                 if(body_obj != NULL)
//                     cJSON_Delete(body_obj);
//                 return false;
//             }
//             for(cJSON* id_obj = body_obj->child; id_obj != NULL; id_obj = id_obj->next)
//             {
//                 body_map::iterator body_it = uri_it->second->find(id_obj->string);
//                 if(body_it == uri_it->second->end())
//                 {
//                     // Value not in our array ignore
//                     continue;
//                 }
//                 update_register_value(server_map->mb_mapping, body_it->second.first, body_it->second.second, id_obj);
//             }
//             cJSON_Delete(body_obj);
//         }
//         else
//             // Not our message so ignore
//             return true;
//     }
//     return true;
// }


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

std::shared_ptr<IMaster> setupDNP3master (std::shared_ptr<IChannel> channel, const char* mname, sysCfg* ourDB , int localAddr , int remoteAddr)
{
    MasterStackConfig stackConfig;
    // you can override application layer settings for the master here
    // in this example, we've change the application layer timeout to 2 seconds
    stackConfig.master.responseTimeout = TimeDuration::Seconds(2);
    stackConfig.master.disableUnsolOnStartup = true;
    // You can override the default link layer settings here
    // in this example we've changed the default link layer addressing
    stackConfig.link.LocalAddr = localAddr; // 1
    stackConfig.link.RemoteAddr = remoteAddr; //10;
    // Create a new master on a previously declared port, with a
    // name, log level, command acceptor, and config info. This
    // returns a thread-safe interface used for sending commands.
    //auto master = channel->AddMaster("master", // id for logging
                //                     newSOEHandler::Create(ourDB), // callback for data processing
                //                     asiodnp3::DefaultMasterApplication::Create(), // master application instance
                //                     stackConfig // stack configuration
    auto master = channel->AddMaster("master", // id for logging
                                     fpsSOEHandler::Create(ourDB), // callback for data processing  this generates the pub elements when we get data
                                     fpsMasterApplication::Create(ourDB), // master application instance this manages the collection of al the pub elements 
                                     stackConfig // stack configuration
                                    );

    // do an integrity poll (Class 3/2/1/0) once per minute
    //auto integrityScan = master->AddClassScan(ClassField::AllClasses(), TimeDuration::Minutes(1));
    // TODO we need a way to demand this via FIMS
    auto integrityScan = master->AddClassScan(ClassField::AllClasses(), TimeDuration::Seconds(20));
    // do a Class 1 exception poll every 5 seconds
    auto exceptionScan = master->AddClassScan(ClassField(ClassField::CLASS_1), TimeDuration::Seconds(60));
    
    //auto binscan = master->AddAllObjectsScan(GroupVariationID(1,1),
    //                                                               TimeDuration::Seconds(5));
    //auto objscan = master->AddAllObjectsScan(GroupVariationID(30,1),
    //                                                               TimeDuration::Seconds(5));
    //auto objscan2 = master->AddAllObjectsScan(GroupVariationID(30,5),
    //                                                               TimeDuration::Seconds(10));
    // Enable the master. This will start communications.
    master->Enable();
    bool channelCommsLoggingEnabled = true;
    bool masterCommsLoggingEnabled = true;
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
            commands.Add<ControlRelayOutputBlock>({WithIndex(ControlRelayOutputBlock(ControlCodeFromType(db->crob)), db->offset)});
            break;
        }
        case AnIn16:
        {
            commands.Add<AnalogOutputInt16>({WithIndex(AnalogOutputInt16(db->anInt16), db->offset)});
            break;
        }
        case AnIn32:
        {
            commands.Add<AnalogOutputInt32>({WithIndex(AnalogOutputInt32(db->anInt32), db->offset)});
            break;
        }
        case AnF32:
        {
            commands.Add<AnalogOutputFloat32>({WithIndex(AnalogOutputFloat32(db->anF32), db->offset)});
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
    const char *sub_array[]={
        (const char *)"/interfaces",
        (const char*)"/components",
        (const char*)"/fooey",
        NULL
        };
    //char* uri;
    bool publish_only[3] = {false,false,false};
    bool running = true;
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    //server_data *server_map = NULL;
    //int header_length,;
    //int serial_fd;
    //int fims_socket;
    int fd_max = 0;
    int rc = 0;
    //int server_socket = -1;
    int fims_connect = 0;
    fd_set all_connections;
    FD_ZERO(&all_connections);
    sysCfg sys_cfg;
    
    //memset(&sys_cfg, 0, sizeof(sysCfg));
    datalog data[Num_Register_Types];
    memset(data, 0, sizeof(datalog) * Num_Register_Types);

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
   
    sys_cfg.showDbMap();
    sys_cfg.showUris();

    sys_cfg.p_fims = p_fims;// = new fims();
    if (p_fims == NULL)
    {
        FPS_ERROR_PRINT("Failed to allocate connection to FIMS server.\n");
        rc = 1;
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
        rc = 1;
        return 1;
        //goto cleanup;
    }

    FPS_DEBUG_PRINT("Map configured: Initializing data.\n");
     if(p_fims->Subscribe((const char**)sub_array, 3, (bool *)publish_only) == false)
    {
        FPS_ERROR_PRINT("Subscription failed.\n");
        p_fims->Close();
        return 1;
    }

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
    auto master = setupDNP3master (channel, "master1", &sys_cfg , sys_cfg.local_address, sys_cfg.remote_address /*RemoteAddr*/);
    if (!master){
        FPS_ERROR_PRINT("Error in setupDNP3master.\n");
        delete manager;
        return 1;
    }
    //we cant do this
    //auto master2 = setupDNP3master (channel2, "master2", ourDB , 2 /*localAddr*/ , 10 /*RemoteAddr*/);

    FPS_DEBUG_PRINT("DNP3 Setup complete: Entering main loop.\n");

    //sys_cfg.p_fims = new fims();

    
    // send out initial sunscribes
    //ssys_cfg.subsUris();
    // send out intial gets
    // set max ticks
    sys_cfg.getUris("master");
    // set all values to inval  done at the start
    // start time to complete gets
    // TODO set for all the getURI responses as todo
    // done only get outstation vars 

    //build/release/fims_send -m set -u "/dnp3/master" '{"type":"analog32","offset":4,"value":38}'
    //fims_send -m set -u "/components/<some_master_id>/<some_outstation_id> '{"AnalogInt32": [{"offset":"name_or_index","value":52},{"offset":2,"value":5}]}' 
    // AnalogOutputInt16 ao(100);
    // AnalogOutputInt16 a1(101);
    // AnalogOutputInt16 a2(102);
    // AnalogOutputInt16 a3(103);
    // master->DirectOperate(CommandSet(
    //             {WithIndex(ao, 2),
    //             WithIndex(a1, 3),
    //             WithIndex(a2, 4),
    //             WithIndex(a3, 5)}
    //             ), PrintingCommandCallback::Get());
    // master->SelectAndOperate(CommandSet(
    //             {WithIndex(ao, 2),
    //             WithIndex(a1, 3),
    //             WithIndex(a2, 4),
    //             WithIndex(a3, 5)}
    //             ), PrintingCommandCallback::Get());

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
                    addVarToCj(cj, dbp);
                    dbs.pop_back();
                }

                if(cj)
                {
                    const char* reply = cJSON_PrintUnformatted(cj);
                    cJSON_Delete(cj);
                    cj = NULL;
                    p_fims->Send("set", msg->replyto, NULL, reply);
                    free((void* )reply);
                }
                FPS_DEBUG_PRINT("      *****Running Direct Outputs \n");
                master->DirectOperate(std::move(commands), PrintingCommandCallback::Get());
            }

            if (cjb != NULL)
            {
                cJSON_Delete(cjb);
            }
            p_fims->free_message(msg);
        }
    }
       
    // if (0)
    // {
    //     fd_set connections_with_data;
    //     fims_socket = sys_cfg.p_fims->get_socket();

    //     if(fims_socket != -1)
    //         FD_SET(fims_socket, &all_connections);
    //     else
    //     {
    //         FPS_ERROR_PRINT("Failed to get fims socket.\n");
    //         rc = 1;
    //         goto cleanup;
    //     }

    //     fd_max = (fd_max > fims_socket) ? fd_max: fims_socket;
    //     FPS_DEBUG_PRINT("Fims Setup complete: now for DNP3\n");

    //     while(running)
    //     {
    //         connections_with_data = all_connections;
    //         // Select will block until one of the file descriptors has data
    //         if(-1 == select(fd_max+1, &connections_with_data, NULL, NULL, NULL))
    //         {
    //             FPS_ERROR_PRINT("server select() failure: %s.\n", strerror(errno));
    //             break;
    //         }
    //         //Loop through file descriptors to see which have data to read
    //         for(int current_fd = 0; current_fd <= fd_max; current_fd++)
    //         {
    //             // if no data on this file descriptor skip to the next one
    //             if(!FD_ISSET(current_fd, &connections_with_data))
    //                 continue;
    //             if(current_fd == fims_socket)
    //             {
    //                 // Fims message received
    //                 fims_message* msg = server_map->p_fims->Receive_Timeout(1*MICROSECOND_TO_MILLISECOND);
    //                 if(msg == NULL)
    //                 {
    //                     if(server_map->p_fims->Connected() == false)
    //                     {
    //                         // fims connection closed
    //                         FPS_ERROR_PRINT("Fims connection closed.\n");
    //                         FD_CLR(current_fd, &all_connections);
    //                         break;
    //                     }
    //                     else
    //                         FPS_ERROR_PRINT("No fims message. Select led us to a bad place.\n");
    //                 }
    //                 else
    //                 {
    //                     process_fims_message(msg, server_map);
    //                     server_map->p_fims->free_message(msg);
    //                 }
    //             }
    //         }
    //     }

    //     FPS_DEBUG_PRINT("Main loop complete: Entering clean up.\n");
    // }

    //cleanup:
    if (manager) delete manager;
    // sys_cfg should clean itself up
 
    //if(sys_cfg.eth_dev       != NULL) free(sys_cfg.eth_dev);
    //if(sys_cfg.ip_address    != NULL) free(sys_cfg.ip_address);
    //if(sys_cfg.name          != NULL) free(sys_cfg.name);
    //if(sys_cfg.serial_device != NULL) free(sys_cfg.serial_device);
    //if(sys_cfg.mb != NULL)             modbus_free(sys_cfg.mb);
    for(int fd = 0; fd < fd_max; fd++)
        if(FD_ISSET(fd, &all_connections))
            close(fd);
    return rc;
}


