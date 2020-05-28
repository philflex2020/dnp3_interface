/*
* dnp3_outstation
*/

#include <fims/libfims.h>
#include <string.h>
#include <fims/fps_utils.h>
#include <cjson/cJSON.h>
#include <unistd.h>
#include "dnp3_utils.h"

#include <iostream>
#include <string>
#include <thread>
#include <stdio.h>

#include <opendnp3/LogLevels.h>
#include <opendnp3/outstation/IUpdateHandler.h>

#include <asiodnp3/DNP3Manager.h>
#include <asiodnp3/UpdateBuilder.h>
#include "fpsCommandHandler.h"
#include "fpsLogger.h"
#include "fpsChannelListener.h"
#include "fpsOutstationApplication.h"


using namespace std;
using namespace opendnp3;
using namespace openpal;
using namespace asiopal;
using namespace asiodnp3;
#define MICROSECOND_TO_MILLISECOND 1000
#define NANOSECOND_TO_MILLISECOND  1000000
#define MAX_FIMS_CONNECT 5

fims *p_fims;
//TODO fill out from system config
void ConfigureDatabase(DatabaseConfig& config, sysCg* ourDB)
{
    // just deal with analog vars and Group30Var5, this allows floating point numbers through the system
    auto dsize = ourDB->dbMapIx[Type_Analog].size();
    for (auto i = 0; i < dsize; i++)
    {
        DbVar* db = ourDB->getDbVarId(Type_Analog, i);
        if(db != NULL)
        {
            if(db->variation == Group30Var5)
            {
                config.analog[i].svariation = StaticAnalogVariation::Group30Var5;
            }
        }
    }   
    // example of configuring analog index 0 for Class2 with floating point variations by default
    //config.analog[0].clazz = PointClass::Class2;
    //config.analog[0].svariation = StaticAnalogVariation::Group30Var5;
    //config.analog[0].evariation = EventAnalogVariation::Group32Var7;
    //config.analog[0].deadband = 1.0; ///EventAnalogVariation::Group32Var7;   
}

DNP3Manager* setupDNP3Manager(sysCfg* ourDB)
{
    auto manager = new DNP3Manager(1, fpsLogger::Create(ourDB));
    return manager;
}

std::shared_ptr<IChannel> setupDNP3channel(DNP3Manager* manager, const char* cname, sysCfg* ourDB, const char* addr, int port) {
    // Specify what log levels to use. NORMAL is warning and above
    // You can add all the comms logging by uncommenting below.
    const uint32_t FILTERS = levels::NORMAL;// | levels::ALL_COMMS;

    // Create a TCP server (listener)
    auto channel = manager->AddTCPServer(cname, 
                                        FILTERS, 
                                        ServerAcceptMode::CloseExisting, 
                                        addr, 
                                        port,
                                        fpsChannelListener::Create(ourDB)
                                        );
//    cout << "channel0 set up!\n";
    return channel;
}

std::shared_ptr<IOutstation> setupDNP3outstation (std::shared_ptr<IChannel> channel, const char* mname, sysCfg* ourDB, int localAddr, int remoteAddr)
{
    // The main object for a outstation. The defaults are useable,
    // but understanding the options are important.
    //OutstationStackConfig config(DatabaseSizes::AllTypes(10));
    //OutstationStackConfig config(DatabaseSizes::AllTypes(10));
    cout<<"Binaries: "<<ourDB->dbMapIx[Type_Binary].size()<<" Analogs: "<<ourDB->dbMapIx[Type_Analog].size()<<endl;
    OutstationStackConfig config(DatabaseSizes(ourDB->dbMapIx[Type_Binary].size(),
                                                0,
                                                ourDB->dbMapIx[Type_Analog].size(),
                                                0,0,0,0,0,0));

    // Specify the maximum size of the event buffers. Defaults to 0
    // config.outstation.eventBufferConfig = EventBufferConfig::AllTypes(10);
    config.outstation.eventBufferConfig.maxBinaryEvents = ourDB->dbMapIx[Type_Binary].size(),
    config.outstation.eventBufferConfig.maxAnalogEvents = ourDB->dbMapIx[Type_Analog].size(),
    // Specify the maximum size of the event buffers
    //config.outstation.eventBufferConfig = EventBufferConfig::AllTypes(10);

    // you can override an default outstation parameters here
    // in this example, we've enabled the oustation to use unsolicted reporting
    // if the master enables it
    config.outstation.params.allowUnsolicited = true;

    // You can override the default link layer settings here
    // in this example we've changed the default link layer addressing
    config.link.LocalAddr = 10;//localAddr;
    config.link.RemoteAddr = 1;//remoteAddr;

    config.link.KeepAliveTimeout = openpal::TimeDuration::Max();

    // You can optionally change the default reporting variations or class assignment prior to enabling the outstation
    ConfigureDatabase(config.dbConfig, ourDB);

    // Create a new outstation with a log level, command handler, and
    // config info this	returns a thread-safe interface used for
    // updating the outstation's database.
    // TODO fpsOutStationApplication
    auto outstation = channel->AddOutstation("outstation", 
                                            fpsCommandHandler::Create(ourDB),
                                            fpsOutstationApplication::Create(ourDB), config);

    // Enable the outstation and start communications
    outstation->Enable();
    return outstation;
}

void addVarToBuilder (UpdateBuilder& builder, DbVar *db)
{
    switch (db->type) 
    {
        case Type_Analog:
        {
            builder.Update(Analog(db->valuedouble), db->offset);
            break;
        }
        case Type_Binary:
        {
            builder.Update(Binary(db->valueint), db->offset);
            break;
        }
        default:
            break;
    }
}

int main(int argc, char* argv[])
{
    sysCfg sys_cfg;
    int rc = 0;
    int fims_connect = 0;
    int ttick = 0; // timeout tick
    p_fims = new fims();
    const char *sub_array[]={
        (const char *)"/interfaces",
        (const char*)"/components",
        (const char*)"/fooey",
        NULL
        };
    
    bool publish_only[3] = {false,false,false};
    bool running = true;
    
    cJSON* config = get_config_json(argc, argv);
    if(config == NULL)
    {
        fprintf(stderr, "Error reading config file system.\n");
        return 1;
    }

    if(!parse_system(config, &sys_cfg)) 
    {
        fprintf(stderr, "Error reading config file system.\n");
        cJSON_Delete(config);
        return 1;
    }

    if(!parse_variables(config, &sys_cfg)) 
    {
        fprintf(stderr, "Error reading config file variables.\n");
        cJSON_Delete(config);
        return 1;
    }

    sys_cfg.showDbMap();
    sys_cfg.showUris();

    cJSON_Delete(config);

    sys_cfg.p_fims = p_fims = new fims();

    if (p_fims == NULL)
    {
        FPS_ERROR_PRINT("Failed to allocate connection to FIMS server.\n");
        rc = 1;
        return 1;
        //goto cleanup;
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

    if(p_fims->Subscribe((const char**)sub_array, 3, (bool *)publish_only) == false)
    {
        FPS_ERROR_PRINT("Subscription failed.\n");
        p_fims->Close();
        return 1;
    }

    // Main point of interaction with the stack. 1 thread in the pool for 1 outstation
    // Manager must be in main scope
    auto manager = setupDNP3Manager(&sys_cfg);
    if (!manager)
    {
        FPS_ERROR_PRINT( "DNP3 Manger setup failed.\n");
        return 1;
    }

    auto channel = setupDNP3channel(manager, "server", &sys_cfg, sys_cfg.ip_address, sys_cfg.port);
    if (!channel){
        FPS_ERROR_PRINT( "DNP3 Channel setup failed.\n");
        return 1;
    }

    auto outstation = setupDNP3outstation(channel, "outstation", &sys_cfg, sys_cfg.local_address, sys_cfg.remote_address);
    if (!outstation){
        FPS_ERROR_PRINT( "Outstation setup failed.\n");
        return 1;
    }

    FPS_ERROR_PRINT( "DNP3 Setup complete: Entering main loop.\n");

    // // send out initial sunscribes
    //ssys_cfg.subsUris();
    // send out intial gets
    // set max ticks
    sys_cfg.getUris("outstation");
    // set all values to inval  done at the start
    // start time to complete gets
    // TODO set for all the getURI responses as todo
    // done only get outstation vars 
    //
    while(running && p_fims->Connected())
    {
        // use a time out to detect init failure 
        fims_message* msg = p_fims->Receive_Timeout(1000000);
        if(msg == NULL)
        { 
            // TODO check for all the getURI resposes
            FPS_DEBUG_PRINT("Timeout tick %d\n", ttick);
            ttick++;
            bool ok = sys_cfg.checkUris("outstation");
            if(ok == false)
            {
                if (ttick > MAX_SETUP_TICKS)
                {
                    // just quit here
                    FPS_DEBUG_PRINT("QUITTING TIME Timeout tick %d\n", ttick);

                }
            }
        }
        else
        {

            //FPS_ERROR_PRINT("****** Hey %s got a message uri [%s] \n", __FUNCTION__, msg->uri);
            dbs_type dbs; // collect all the parsed vars here

            cJSON* cjb = parseBody(dbs, &sys_cfg, msg, "outstation");
            if(dbs.size() > 0)
            {
                cJSON* cj = NULL;                
                if(msg->replyto != NULL)
                    cj = cJSON_CreateObject();
                    
                UpdateBuilder builder;
                while (!dbs.empty())
                {
                    std::pair<DbVar*,int>dbp = dbs.back();
                    DbVar* db = dbp.first;
                    addVarToBuilder(builder, db);
                    addVarToCj(cj, dbp);  // include flag
                    dbs.pop_back();
                }

                //finalize set of updates
                auto updates = builder.Build();
                outstation->Apply(updates);

                if(cj)
                {
                    if(msg->replyto)
                    {
                        const char* reply = cJSON_PrintUnformatted(cj);
                        p_fims->Send("set", msg->replyto, NULL, reply);
                        free((void* )reply);
                    }
                    cJSON_Delete(cj);
                    cj = NULL;
                }
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

    if(sys_cfg.ip_address    != NULL) free(sys_cfg.ip_address);
    if(sys_cfg.name          != NULL) free(sys_cfg.name);
    return rc;
}