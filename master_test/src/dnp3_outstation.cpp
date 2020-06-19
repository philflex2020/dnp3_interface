/*
* dnp3_outstation
*/

#include <fims/libfims.h>
#include <string.h>
#include <stdint.h>

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
void ConfigureDatabase(DatabaseConfig& config, sysCfg* sys)
{
    // just deal with analog vars and Group30Var5, this allows floating point numbers through the system
    auto dsize = sys->dbVec[Type_Analog].size();
    for (int i = 0; i < static_cast<int32_t>(dsize); i++)
    {
        DbVar* db = sys->getDbVarId(Type_Analog, i);
        if(db != NULL)
        {
            if(db->variation == Group30Var5)
            {
                config.analog[i].svariation = StaticAnalogVariation::Group30Var5;
            }
            else if(db->variation == Group30Var2)
            {
                config.analog[i].svariation = StaticAnalogVariation::Group30Var2;
            }
            if(db->evariation == Group32Var7)
            {
                config.analog[i].evariation = EventAnalogVariation::Group32Var7;
            }

            if(db->clazz != 0)
            {
                switch (db->clazz ) 
                {
                    case 1:
                    {
                        config.analog[i].clazz = PointClass::Class1;
                        break;
                    }
                    case 2:
                    {
                        config.analog[i].clazz = PointClass::Class2;
                        break;
                    }
                    case 3:
                    {
                        config.analog[i].clazz = PointClass::Class3;
                        break;
                    }
                    default:
                        break;
                }
            }
            // else if(db->variation == Group30Var2)
            // {
            //     config.analog[i].svariation = StaticAnalogVariation::Group30Var2;
            // }
        }
    }   
    // example of configuring analog index 0 for Class2 with floating point variations by default
    //config.analog[0].clazz = PointClass::Class2;
    //config.analog[0].svariation = StaticAnalogVariation::Group30Var5;
    //config.analog[0].evariation = EventAnalogVariation::Group32Var7;
    //config.analog[0].deadband = 1.0; ///EventAnalogVariation::Group32Var7;   
}

DNP3Manager* setupDNP3Manager(sysCfg* sys)
{
    auto manager = new DNP3Manager(1, fpsLogger::Create(sys));
    return manager;
}

std::shared_ptr<IChannel> setupDNP3channel(DNP3Manager* manager, const char* cname, sysCfg* sys) {
    // Specify what log levels to use. NORMAL is warning and above
    // You can add all the comms logging by uncommenting below.
    const uint32_t FILTERS = levels::NORMAL | levels::ALL_COMMS;

    // Create a TCP server (listener)
    auto channel = manager->AddTCPServer(cname, 
                                        FILTERS, 
                                        ServerAcceptMode::CloseExisting, 
                                        sys->ip_address, 
                                        sys->port,
                                        fpsChannelListener::Create(sys)
                                        );
    return channel;
}

void setConfigUnsol(sysCfg* sys, OutstationStackConfig &config)
{
    if(sys->unsol >= 0)
    {
        if(sys->unsol == 0)
            config.outstation.params.allowUnsolicited = false;
        else
            config.outstation.params.allowUnsolicited = true;
        sys->unsol = -1;
    }
}

std::shared_ptr<IOutstation> setupDNP3outstation (std::shared_ptr<IChannel> channel, const char* mname, sysCfg* sys, OutstationStackConfig &config)
{
    // The main object for a outstation. The defaults are useable,
    // but understanding the options are important.
    //OutstationStackConfig config(DatabaseSizes::AllTypes(10));
    //OutstationStackConfig config(DatabaseSizes::AllTypes(10));
    // cfgdb.binary.size(),  // binary input
	// 	0,                     // double binary input
	// 	cgdb.analog.size(),    // analog  input
	// 	0,                     // counter input
	// 	0,                     // frozen counter
	// 	cfgdb.binaryop.size(), // binary output status 
	// 	0,                     // analog output status
	// 	0,                     // time and intervat
	// 	0                      // octet string
    // cout<<"Binaries: "<<fpsDB->dbVec[Type_Binary].size()<<" Analogs: "<<fpsDB->dbVec[Type_Analog].size()<<endl;
    // OutstationStackConfig config(DatabaseSizes( fpsDB->dbVec[Type_Binary].size(),
    //                                             0,
    //                                             fpsDB->dbVec[Type_Analog].size(),
    //                                             0,
    //                                             0,
    //                                             fpsDB->dbVec[Type_BinaryOS].size(),
    //                                             fpsDB->dbVec[Type_AnalogOS].size(),
    //                                             0,
    //                                             0));

    // Specify the maximum size of the event buffers. Defaults to 0
    config.outstation.eventBufferConfig = EventBufferConfig::AllTypes(10);
    //config.outstation.eventBufferConfig.maxBinaryEvents = fpsDB->dbVec[Type_Binary].size(),
    //config.outstation.eventBufferConfig.maxAnalogEvents = fpsDB->dbVec[Type_Analog].size(),
    // Specify the maximum size of the event buffers
    //config.outstation.eventBufferConfig = EventBufferConfig::AllTypes(10);

    // you can override an default outstation parameters here
    // in this example, we've enabled the oustation to use unsolicted reporting
    // if the master enables it
    config.outstation.params.allowUnsolicited = true;
    sys->unsol = 1;
    // allow sys to over rule
    setConfigUnsol(sys, config);

    //config.outstation.params.allowUnsolicited = false;

    // You can override the default link layer settings here
    // in this example we've changed the default link layer addressing
    // 
    config.link.LocalAddr = sys->local_address;   // was 10
    config.link.RemoteAddr = sys->remote_address;//  was 1;

    config.link.KeepAliveTimeout = openpal::TimeDuration::Max();

    // You can optionally change the default reporting variations or class assignment prior to enabling the outstation
    ConfigureDatabase(config.dbConfig, sys);

    // Create a new outstation with a log level, command handler, and
    // config info this	returns a thread-safe interface used for
    // updating the outstation's database.
    // TODO fpsOutStationApplication
    auto outstation = channel->AddOutstation("outstation" 
                                            , fpsCommandHandler::Create(sys)
                                            , fpsOutstationApplication::Create(sys)
                                            , config);

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
            builder.Update(Analog(db->valuedouble), db->idx);
            break;
        }
        case Type_AnalogOS:
        {
            builder.Update(AnalogOutputStatus(db->valuedouble), db->idx);
            break;
        }
        case Type_Binary:
        {
            int32_t vint = static_cast<int32_t>(db->valuedouble);
            builder.Update(Binary(vint), db->idx);
            break;
        }
        case Type_BinaryOS:
        {
            int32_t vint = static_cast<int32_t>(db->valuedouble);
            builder.Update(BinaryOutputStatus(vint), db->idx);
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

    if (strcmp(DNP3_UTILS_VERSION, getVersion())!=0)
    {
        FPS_ERROR_PRINT("Error with installed DNP3_UTILS_VERSION\n");
        return 1;
    }

    p_fims = new fims();
    
    bool running = true;
    
    cJSON* config = get_config_json(argc, argv);
    if(config == NULL)
    {
        fprintf(stderr, "Error reading config file system.\n");
        return 1;
    }

    if(!parse_system(config, &sys_cfg, DNP3_OUTSTATION)) 
    {
        fprintf(stderr, "Error reading config file system.\n");
        cJSON_Delete(config);
        return 1;
    }

    if(!parse_variables(config, &sys_cfg, DNP3_OUTSTATION)) 
    {
        fprintf(stderr, "Error reading config file variables.\n");
        cJSON_Delete(config);
        return 1;
    }

    sys_cfg.showDbMap();
    sys_cfg.showNewUris();
    
    cJSON_Delete(config);

    const char **subs = NULL;
    bool *bpubs = NULL;
    int num = getSysUris(&sys_cfg, DNP3_OUTSTATION, subs, bpubs);
    if(num < 0)
    {
        FPS_ERROR_PRINT("Failed to create subs array.\n");
        return 1;
    }

    FPS_ERROR_PRINT(">>>>>> Num Uris found %d .\n", num);
    for (int ix = 0; ix < num; ix++ )
    {
         FPS_ERROR_PRINT(">>>>>>>>>> Uri [%d] [%s] \n", ix, subs[ix]);    
    }

    sys_cfg.p_fims = p_fims = new fims();

    if (p_fims == NULL)
    {
        FPS_ERROR_PRINT("Failed to allocate connection to FIMS server.\n");
        rc = 1;
        return 1;
        //goto cleanup;
    }
    // use the id for fims connect but also add outsttion designation 
    {
        char tmp[1024];
        snprintf(tmp, sizeof(tmp),"DNP3_O_%s", sys_cfg.id);
        while(fims_connect < MAX_FIMS_CONNECT && p_fims->Connect(tmp) == false)
        {
            fims_connect++;
            sleep(1);
        }
    }

    if(fims_connect >= MAX_FIMS_CONNECT)
    {
        FPS_ERROR_PRINT("Failed to establish connection to FIMS server.\n");
        rc = 1;
        return 1;
        //goto cleanup;
    } 

    if(p_fims->Subscribe(subs, num, bpubs) == false)
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

    auto channel = setupDNP3channel(manager, "server", &sys_cfg);
    if (!channel){
        FPS_ERROR_PRINT( "DNP3 Channel setup failed.\n");
        return 1;
    }
    // TODO fix this
    sysCfg* sys = &sys_cfg;
    cout<<"Binaries: "<<sys->dbVec[Type_Binary].size()<<" Analogs: "<<sys->dbVec[Type_Analog].size()<<endl;
    OutstationStackConfig OSconfig(DatabaseSizes( 
                                                sys->getTypeSize(Type_Binary),
                                                0,                               // no double binaries
                                                sys->getTypeSize(Type_Analog),
                                                0,                               // no counters
                                                0,                               // no frozen counters
                                                sys->getTypeSize(Type_BinaryOS),
                                                sys->getTypeSize(Type_AnalogyOS),
                                                0,                               // no timers
                                                0                                // no octet streams
                                                )); 

    auto outstation = setupDNP3outstation(channel, "outstation", sys, OSconfig);
    if (!outstation){
        FPS_ERROR_PRINT( "Outstation setup failed.\n");
        return 1;
    }

    FPS_ERROR_PRINT( "DNP3 Setup complete: Entering main loop.\n");

    // // send out initial subscribes
    //ssys_cfg.subsUris();
    // send out intial gets
    // set max ticks
    sys->getUris(DNP3_OUTSTATION);
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
            bool ok = sys->checkUris(DNP3_OUTSTATION);
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
            if (sys->debug == 1)
                FPS_ERROR_PRINT("****** Hey %s got a message uri [%s] \n", __FUNCTION__, msg->uri);
            dbs_type dbs; // collect all the parsed vars here

            cJSON* cjb = parseBody(dbs, sys, msg, DNP3_OUTSTATION);
            if(dbs.size() > 0)
            {
                cJSON* cj = NULL;                
                if((msg->replyto != NULL) && (strcmp(msg->method, "pub") != 0))
                    cj = cJSON_CreateObject();
                    
                UpdateBuilder builder;
                int varcount = 0;
                while (!dbs.empty())
                {
                    std::pair<DbVar*,int>dbp = dbs.back();
                    DbVar* db = dbp.first;
                    // only do this on sets pubs or  posts
                    if (
                        (strcmp(msg->method, "set") == 0) || 
                        (strcmp(msg->method, "post") == 0) || 
                        (strcmp(msg->method, "pub") == 0)
                        )
                        {
                            varcount++;
                            addVarToBuilder(builder, db);
                        }
                    addVarToCj(cj, dbp);  // include flag
                    dbs.pop_back();
                }

                //finalize set of updates
                if(varcount > 0) 
                {
                    auto updates = builder.Build();
                    outstation->Apply(updates);
                }
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

            if (sys->scanreq > 0)
            {
                FPS_ERROR_PRINT("****** outstation scanreq %d ignored \n", sys->scanreq);
                sys->scanreq = 0;
            }

            if (sys->unsol >= 0)
            {
                FPS_ERROR_PRINT("****** outstation unsol %d handled \n", sys->unsol);
                setConfigUnsol(sys, OSconfig);                
            }

            if (sys->cjclass != NULL)
            {
                const char*tmp = cJSON_PrintUnformatted(sys->cjclass);
                if(tmp != NULL)
                {                
                    FPS_ERROR_PRINT("****** outstation class change [%s] handled \n", tmp);
                    free((void*)tmp);
                    sys->cjclass = NULL;
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