#include <fims/libfims.h>
#include <string.h>
#include <fims/fps_utils.h>
#include <cjson/cJSON.h>
#include <unistd.h>
#include "dnp3_utils.h"
// #include </home/vagrant/git/dnp3_interface/include/Value_Object.h>
// #include </home/vagrant/git/dnp3_interface/include/Fims_Object.h>

#include <iostream>
#include <string>
#include <thread>
#include <stdio.h>

#include <opendnp3/LogLevels.h>
#include <opendnp3/outstation/IUpdateHandler.h>
#include <opendnp3/outstation/SimpleCommandHandler.h>

#include <asiodnp3/ConsoleLogger.h>
#include <asiodnp3/DNP3Manager.h>
#include <asiodnp3/PrintingChannelListener.h>
#include <asiodnp3/PrintingSOEHandler.h>
#include <asiodnp3/UpdateBuilder.h>
#include "newCommandHandler.h"

using namespace std;
using namespace opendnp3;
using namespace openpal;
using namespace asiopal;
using namespace asiodnp3;
#define MICROSECOND_TO_MILLISECOND 1000
#define NANOSECOND_TO_MILLISECOND  1000000
#define MAX_FIMS_CONNECT 5

fims *p_fims;

char *binary_names[5];
int *binary_indices[5];
char *analog_names[5];
int *analog_indices[5];

void ConfigureDatabase(DatabaseConfig& config)
{
    // example of configuring analog index 0 for Class2 with floating point variations by default
    config.analog[0].clazz = PointClass::Class2;
    config.analog[0].svariation = StaticAnalogVariation::Group30Var5;
    config.analog[0].evariation = EventAnalogVariation::Group32Var7;
    config.analog[0].deadband = 1.0; ///EventAnalogVariation::Group32Var7;
    
}

struct State
{
    uint32_t count = 0;
    double value = 0;
    bool binary = false;
    DoubleBit dbit = DoubleBit::DETERMINED_OFF;
    uint8_t octetStringValue = 1;
};

DNP3Manager* setupDNP3Manager(void)
{
    auto manager = new DNP3Manager(1, ConsoleLogger::Create());
    return manager;
}

std::shared_ptr<IChannel> setupDNP3channel(DNP3Manager* manager, const char* cname, const char* addr, int port) {
    // Specify what log levels to use. NORMAL is warning and above
    // You can add all the comms logging by uncommenting below.
    const uint32_t FILTERS = levels::NORMAL | levels::ALL_COMMS;

    // Create a TCP server (listener)
    auto channel = manager->AddTCPServer(cname, FILTERS, ServerAcceptMode::CloseExisting, addr, port,
                                        PrintingChannelListener::Create());
    cout << "channel0 set up!\n";
    return channel;
}

std::shared_ptr<IOutstation> setupDNP3outstation (std::shared_ptr<IChannel> channel, const char* mname, sysCfg* ourDB , int localAddr , int remoteAddr)
{
    // The main object for a outstation. The defaults are useable,
    // but understanding the options are important.
    OutstationStackConfig config(DatabaseSizes::AllTypes(10));

    // Specify the maximum size of the event buffers
    config.outstation.eventBufferConfig = EventBufferConfig::AllTypes(10);

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
    ConfigureDatabase(config.dbConfig);

    // Create a new outstation with a log level, command handler, and
    // config info this	returns a thread-safe interface used for
    // updating the outstation's database.
    auto outstation = channel->AddOutstation("outstation", newCommandHandler::Create(ourDB),
                                             DefaultOutstationApplication::Create(), config);

    // Enable the outstation and start communications
    outstation->Enable();
    return outstation;
}

void addVarToBuilder (UpdateBuilder& builder, DbVar *db)
{
    switch (db->type) 
    {
        // case Type_Crob:
        // {
        //     builder.Update(Binary(db->crob), db->offset);
        //     break;
        // }
        // case AnIn16:
        // {
        //     builder.Update(Analog(db->anInt16), db->offset);
        //     break;
        // }
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
    p_fims = new fims();
    char **sub_array = new char*[2];
    sub_array[0] = (char *)"/components";
    sub_array[1] = (char *) "/interfaces";
    bool publish_only = false;
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

    // Main point of interaction with the stack. 1 thread in the pool for 1 outstation
    // Manager must be in main scope
    auto manager = setupDNP3Manager();
    if (!manager)
    {
        fprintf(stderr, "DNP3 Manger setup failed.\n");
        return 1;
    }

    auto channel = setupDNP3channel(manager, "server", sys_cfg.ip_address, sys_cfg.port);
    if (!channel){
        fprintf(stderr, "DNP3 Channel setup failed.\n");
        return 1;
    }

    auto outstation = setupDNP3outstation(channel, "outstation", &sys_cfg, sys_cfg.local_address, sys_cfg.remote_address);
    if (!outstation){
        fprintf(stderr, "Outstation setup failed.\n");
        return 1;
    }

    fprintf(stderr, "DNP3 Setup complete: Entering main loop.\n");

    sys_cfg.p_fims = p_fims = new fims();

    if (p_fims == NULL)
    {
        FPS_ERROR_PRINT("Failed to allocate connection to FIMS server.\n");
        rc = 1;
        goto cleanup;
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
        goto cleanup;
    } 
    //TO dodo interfaces   
    // subs = /components
    if(p_fims->Subscribe((const char**)sub_array, 2, (bool *)&publish_only) == false)
    {
        FPS_ERROR_PRINT("Subscription failed.\n");
        p_fims->Close();
        return 1;
    }
    // send out initial sunscribes
    //ssys_cfg.subsUris();
    // send out intial gets
    sys_cfg.getUris();
    // set all values to inval
    // start time to complete gets
    //
    while(running && p_fims->Connected())
    {
        fims_message* msg = p_fims->Receive();
        if(msg != NULL)
        {

            FPS_ERROR_PRINT("****** Hey %s got a message uri [%s] \n", __FUNCTION__, msg->uri);
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

    cleanup:
    if (manager) delete manager;

    if(sys_cfg.ip_address    != NULL) free(sys_cfg.ip_address);
    if(sys_cfg.name          != NULL) free(sys_cfg.name);
    return rc;
}