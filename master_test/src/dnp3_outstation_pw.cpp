/* 
 *  dnp3 outstation test code  
 *  pwilshire / pmiller
 *  5/11/2020
 */

#include <iostream>
#include <string>
#include <thread>
#include <stdio.h>
#include <stdint.h>

#include <fims/libfims.h>
#include <string.h>
#include <fims/fps_utils.h>
#include <cjson/cJSON.h>
#include <fims/libfims.h>

#include <opendnp3/LogLevels.h>
#include <opendnp3/outstation/IUpdateHandler.h>
#include <opendnp3/outstation/SimpleCommandHandler.h>

#include <asiodnp3/ConsoleLogger.h>
#include <asiodnp3/DNP3Manager.h>
#include <asiodnp3/PrintingChannelListener.h>
#include <asiodnp3/PrintingSOEHandler.h>
#include <asiodnp3/UpdateBuilder.h>

#include "newCommandHandler.h"
#include "newOutstationApplication.h"

#include "dnp3_utils.h"

using namespace std;
using namespace opendnp3;
using namespace openpal;
using namespace asiopal;
using namespace asiodnp3;

fims *p_fims;

void ConfigureDatabase(DatabaseConfig& config)
{
    std::cout <<" before changes svariation :"<<(int) config.analog[0].svariation<<"\n";
    std::cout <<" before changes evariation :"<<(int) config.analog[0].evariation<<"\n";

    // example of configuring analog index 0 for Class2 with floating point variations by default
    config.analog[0].clazz = PointClass::Class2;
    config.analog[0].svariation = StaticAnalogVariation::Group30Var5;
    config.analog[1].svariation = StaticAnalogVariation::Group30Var5;
    config.analog[0].evariation = EventAnalogVariation::Group32Var7;
    std::cout <<" after changes svariation :"<<(int) config.analog[0].svariation<<"\n";
    std::cout <<" after changes evariation :"<<(int) config.analog[0].evariation<<"\n";
    //std::cout <<" *****************analog[0] value :"<<(int) config.analog[0].value.value<<"\n";

    //config.analog[1].clazz = PointClass::Class2;
    //config.analog[1].svariation = StaticAnalogVariation::Group30Var5;
    //config.analog[1].evariation = EventAnalogVariation::Group30Var5;
 
}
// keep the state for outputs.
struct State
{
    uint32_t count = 0;
    double value = 0;
    bool binary = false;
    DoubleBit dbit = DoubleBit::DETERMINED_OFF;
    uint8_t octetStringValue = 1;
};

OutstationStackConfig myConfig(DatabaseSizes::AllTypes(10));

std::shared_ptr<asiodnp3::IOutstation> outstation_init(asiodnp3::DNP3Manager *manager, sysCfg* ourDB, OutstationStackConfig& config) {
    // Specify what log levels to use. NORMAL is warning and above
    // You can add all the comms logging by uncommenting below.
    const uint32_t FILTERS = levels::NORMAL | levels::ALL_COMMS;

    // Create a TCP server (listener)
    auto channel = manager->AddTCPServer("server"
                                        , FILTERS
                                        , ServerAcceptMode::CloseExisting
                                        , "127.0.0.1"
                                        , 20001
                                        , PrintingChannelListener::Create()
                                        );

    // The main object for a outstation. The defaults are useable,
    // but understanding the options are important.
    //OutstationStackConfig config(DatabaseSizes::AllTypes(10));

    // Specify the maximum size of the event buffers
    config.outstation.eventBufferConfig = EventBufferConfig::AllTypes(10);

    // you can override an default outstation parameters here
    // in this example, we've enabled the oustation to use unsolicted reporting
    // if the master enables it
    config.outstation.params.allowUnsolicited = true;

    // You can override the default link layer settings here
    // in this example we've changed the default link layer addressing
    config.link.LocalAddr = 10;
    config.link.RemoteAddr = 1;

    config.link.KeepAliveTimeout = openpal::TimeDuration::Max();

    // You can optionally change the default reporting variations or class assignment prior to enabling the outstation
    ConfigureDatabase(config.dbConfig);
    //const auto commandHandler = std::make_shared<newCommandHandler>(MyOutputs);
    // Create a new outstation with a log level, command handler, and
    // config info this	returns a thread-safe interface used for
    // updating the outstation's database.
    //auto outstation = channel->AddOutstation("outstation", SuccessCommandHandler::Create(),
    //                                         DefaultOutstationApplication::Create(), config);
    auto outstation = channel->AddOutstation("outstation"
                                            , newCommandHandler::Create(ourDB)
                                            , newOutstationApplication::Create(ourDB)
                                            , config);
    printf("outstation channel created\n");

    // Enable the outstation and start communications
    std::cout << "Enabling" << std::endl;   
    outstation->Enable();
    std::cout << "outstation channel enabled" << std::endl;
    return outstation;
}

DNP3Manager* setupDNP3Manager(void)
{
    auto manager = new DNP3Manager(1, ConsoleLogger::Create());
    return manager;
}

int main(int argc, char* argv[])
{
    // char *subscriptions[1];
    // int rtn_val = 0;
    // int num_subs = 1;

    char sub[] = "/components/outstation";
    char* subs = sub;
    bool publish_only = false;
    bool running = true;
    sysCfg sys_cfg;
    p_fims = new fims();

    cJSON* config = get_config_json(argc, argv);
    if(config == NULL)
    {
        fprintf(stderr, "Error reading config file system.\n");
        return 1;

    }

    if(!parse_system(config, &sys_cfg)) 
    {
        fprintf(stderr, "Error parsing config file system.\n");
        cJSON_Delete(config);
        return 1;
    }

    if(!parse_variables(config, &sys_cfg)) 
    {
        fprintf(stderr, "Error parsing config file variables.\n");
        cJSON_Delete(config);
        return 1;
    }
    sys_cfg.showBinaries();
    sys_cfg.showAnalogs();
    sys_cfg.showDbMap();

    sys_cfg.p_fims = p_fims;
    // sys_cfg.name, ip_address, port
    cJSON_Delete(config);

    // This is the main point of interaction with the stack
    // Allocate a single thread to the pool since this is a single outstation
    // Must be in main scope
    DNP3Manager *manager = setupDNP3Manager();//(1, ConsoleLogger::Create());
    auto outstation = outstation_init(manager, &sys_cfg, myConfig);
    printf("outstation started\n");

    if(p_fims->Connect((char *)"fims_listen") == false)
    {
        printf("Connect failed.\n");
        p_fims->Close();
        return 1;
    }
    // subs = /components/outstation
    if(p_fims->Subscribe((const char**)&subs, 1, (bool *)&publish_only) == false)
    {
        printf("Subscription failed.\n");
        p_fims->Close();
        return 1;
    }

    while(running && p_fims->Connected())
    {
        const char* uri;
        UpdateBuilder builder;
        fims_message* msg = p_fims->Receive();
        if(msg != NULL)
        {
        // this is all for the set method
        // the get method needs something like this
        // bool Database::Update(const TimeAndInterval& value, uint16_t index)
        // {
        //     auto rawIndex = GetRawIndex<TimeAndIntervalSpec>(index);
        //     auto view = buffers.buffers.GetArrayView<TimeAndIntervalSpec>();

        //     if (view.Contains(rawIndex))
        //     {
        //         view[rawIndex].value = value;
        //         return true;
        //     }
        // }
            bool ok = true;
            cJSON* body_JSON = cJSON_Parse(msg->body);
            cJSON* itype = NULL;
            cJSON* offset = NULL;
            cJSON* body_value = NULL;
            cJSON* iterator = NULL;
            
            if (body_JSON == NULL)
            {
                FPS_ERROR_PRINT("fims message body is NULL or incorrectly formatted: (%s) \n", msg->body);
                ok = false;
            }
            uri = msg->pfrags[2];
            if (strcmp(uri,sys_cfg.id) != 0)
            {
                FPS_ERROR_PRINT("fims message frag 2 [%s] not for this outstation [%s] \n", uri, sys_cfg.id);
                ok = false;
            }

            // 
            // set /dnp3/outstation '{"type":"xx", offset:yy value: zz}'
            // set /dnp3/outstation '{"type":"analog", "offset":01, "value": 2.34}'
            // set /dnp3/outstation '{"values":{"name1":value1, "name2":value2}}'           if (ok) 
            {
                body_value = cJSON_GetObjectItem(body_JSON, "values");
                if (body_value == NULL)
                {
                    FPS_ERROR_PRINT("fims message body values key not found \n");
                }
                else
                {
                    cJSON_ArrayForEach(iterator, body_value) 
                    {
                        std::cout << " Variable name ["<<iterator->string<<"]\n";
                        DbVar * db = sys_cfg.getDbVar(iterator->string);
                        if (db != NULL)
                        {
                            std::cout<< "Found variable type "<<db->type<<"\n";
                            if (db->type == Type_Analog)
                            {
                                builder.Update(Analog(iterator->valuedouble), db->offset);
                            }
                            else if (db->type == Type_Binary)
                            {
                                builder.Update(Binary(iterator->valueint), db->offset);
                            }
                            else 
                            {
                                std::cout << " Variable ["<<iterator->string<<"] type not correct ["<<db->type<<"]\n";
                            }

                        }
                        else
                        {
                            std::cout<< "Error no variable found \n";
                        }
                        
                        //cjvalue = cJSON_GetObjectItem(iterator, "value");
                        //addValueToCommand(&sys_cfg, commands, cjoffset, cjvalue);
                        //commands.Add<AnalogOutputInt16>({WithIndex(AnalogOutputInt16(cjvalue->valueint),dboffset)});
                    }// parse all the values

                    outstation->Apply(builder.Build());

                }
                ok = false;
            }
            if(ok)
            {
                body_value = cJSON_GetObjectItem(body_JSON, "value");
                if (body_value == NULL)
                {
                    FPS_ERROR_PRINT("fims message body value key not found \n");
                    ok = false;
                }
            }
            
            if (ok) 
            {

                offset = cJSON_GetObjectItem(body_JSON, "offset");
                if (offset == NULL)
                {
                    FPS_ERROR_PRINT("fims message body offset key not found \n");
                    ok = false;
                }
            }

            if(ok) 
            {
                itype = cJSON_GetObjectItem(body_JSON, "type");
                if (itype == NULL)
                {
                    FPS_ERROR_PRINT("fims message body type key not found \n");
                    ok = false;
                }
            }

            if(ok) 
            {
                int dboffset = offset->valueint;
                
                if(strcmp(itype->valuestring,"analogs")==0)
                {
                    if(offset->type == cJSON_String) 
                    {
                        dboffset = sys_cfg.getAnalogIdx(offset->valuestring);
                        if(dboffset < 0)
                        {
                            FPS_ERROR_PRINT("fims message body analog variable [%s] not in config\n", offset->valuestring);
                            sys_cfg.showAnalogs();

                        }
                    }
                    if(dboffset >= 0) 
                    {
                        printf("analog offset %d bodyval: %f\n", dboffset, body_value->valuedouble);
                        builder.Update(Analog(body_value->valuedouble), dboffset);
                    }
                }
                else  // default to binary
                {
                    if(offset->type == cJSON_String) 
                    {
                        dboffset = sys_cfg.getBinaryIdx(offset->valuestring);
                        if(dboffset < 0)
                        {
                            FPS_ERROR_PRINT("fims message body binary variable [%s] not in config\n", offset->valuestring);
                            sys_cfg.showBinaries();

                        }
                    }
                    if (dboffset >= 0)
                    {
                        printf("binry offset %d bodyval: %f\n", dboffset, body_value->valuedouble);
                        builder.Update(Binary(body_value->valueint != 0), dboffset);
                    }
                }
                // TODO multiple variables in one message
                outstation->Apply(builder.Build());
            }

            if (body_JSON != NULL)
            {
               cJSON_Delete(body_JSON);
            }
            p_fims->free_message(msg);
            // TODO delete fims message
        }
    }
    delete manager;
}
