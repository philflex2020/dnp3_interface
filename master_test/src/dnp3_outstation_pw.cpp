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

#include "fpsCommandHandler.h"
#include "fpsOutstationApplication.h"

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
    //  you cant do this.... sigh
    //std::cout <<" *****************analog[0] value :"<<config.dbConfig.analog[0].value<<"\n";

    //config.analog[1].clazz = PointClass::Class2;
    //config.analog[1].svariation = StaticAnalogVariation::Group30Var5;
    //config.analog[1].evariation = EventAnalogVariation::Group30Var5;
//  OutstationConfig cfg;
//     OutstationTestObject t(cfg, DatabaseSizes::BinaryOnly(10));

//     {
//         auto view = t.context.GetConfigView();
//         auto setValue = [](Cell<BinarySpec>& cell) -> void {
//             cell.value = Binary(false);
//             cell.config.svariation = StaticBinaryVariation::Group1Var1;
//         };

//         view.binaries.foreach (setValue);
//     }

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
    //const auto commandHandler = std::make_shared<fpsCommandHandler>(MyOutputs);
    // Create a new outstation with a log level, command handler, and
    // config info this	returns a thread-safe interface used for
    // updating the outstation's database.
    //auto outstation = channel->AddOutstation("outstation", SuccessCommandHandler::Create(),
    //                                         DefaultOutstationApplication::Create(), config);
    auto outstation = channel->AddOutstation("outstation"
                                            , fpsCommandHandler::Create(ourDB)
                                            , fpsOutstationApplication::Create(ourDB)
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

    char sub[] = "/components";
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

    if(!parse_system(config, &sys_cfg, DNP3_OUTSTATION)) 
    {
        fprintf(stderr, "Error parsing config file system.\n");
        cJSON_Delete(config);
        return 1;
    }

    if(!parse_variables(config, &sys_cfg, DNP3_OUTSTATION)) 
    {
        fprintf(stderr, "Error parsing config file variables.\n");
        cJSON_Delete(config);
        return 1;
    }
    //sys_cfg.showBinaries();
    //sys_cfg.showAnalogs();
    sys_cfg.showDbMap();
    sys_cfg.showUris();

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
            //cJSON* itype = NULL;
            //cJSON* offset = NULL;
            cJSON* body_value = NULL;
            cJSON* iterator = NULL;
            
            if (body_JSON == NULL)
            {
                FPS_ERROR_PRINT("fims message body is NULL or incorrectly formatted: (%s) \n", msg->body);
                ok = false;
            }
            else
            {
                FPS_ERROR_PRINT("fims message body parsed OK (%s) -> type %d \n", msg->body, body_JSON->type);                
            }
            

            if (msg->nfrags < 2)
            {
                FPS_ERROR_PRINT("fims message not enough pfrags outstation [%s] \n", sys_cfg.id);
                ok = false;
            }
            if(ok)
            {
                uri = msg->pfrags[1];
                if (strncmp(uri,sys_cfg.id,strlen(sys_cfg.id)) != 0)
                {
                    FPS_ERROR_PRINT("fims message frag 1 [%s] not for this outstation [%s] \n", uri, sys_cfg.id);
                    ok = false;
                }
            }
            // 
            // set /components/<id> '{"type":"xx", offset:yy value: zz}'
            // set /components/<id> '{"type":"analog", "offset":01, "value": 2.34}'
            // set /components/<id> '{"values":{"name1":value1, "name2":value2}}'
            ok = false;
            if(strcmp(msg->method,"set") == 0)
            {
                ok = true;
            }

            if(strcmp(msg->method,"pub") == 0)
            {
                FPS_ERROR_PRINT("fims pub [%s] outstation [%s] \n", msg->body, sys_cfg.id);
                // a pub will only service differences perhaps
                //ok = true;
            }

            if(strcmp(msg->method,"get") == 0)
            {
                ok = true;
            }
            if(ok == false)
            {
                FPS_ERROR_PRINT("fims unsupported method [%s] \n", msg->method);
            }

            if (msg->nfrags > 1)
            {
                uri = msg->pfrags[1];
                FPS_ERROR_PRINT("fims message frag 1 [%s] \n", uri);
                if (strncmp(uri, "master", strlen("master")) == 0)
                {
                    FPS_ERROR_PRINT("fims message frag 1 name [%s] not for outstation \n", uri);
                    ok = false;
                }
            }
            if (ok)
            {
                if(strcmp(msg->method,"get") == 0)
                {

                    cJSON* cj = cJSON_CreateObject();

                    if (msg->nfrags > 2)
                    {
                        uri = msg->pfrags[2];
                        FPS_ERROR_PRINT("fims message frag 3 variable name [%s] \n", uri);
                        DbVar *db = sys_cfg.getDbVar(uri);
                        if (db != NULL)
                        {
                            std::cout<< "Found variable type "<<db->type<<"\n";
                            addVarToCj(cj, db);
                        }
                    }
                    else
                    {
                        sys_cfg.addVarsToCj(cj);
                    }
                    
                    const char* reply = cJSON_PrintUnformatted(cj);
                    cJSON_Delete(cj);
                    if(msg->replyto != NULL)
                        p_fims->Send("set", msg->replyto, NULL, reply);
                    free((void* )reply);

                    //don't know when we would receive a get
                    //if(msg->replyto != NULL)
                    //    p_fims->Send("set", msg->replyto, NULL, "Error: Method not implemented for that URI.");
                    //return true;
                    //FPS_ERROR_PRINT("fims method [%s] not yet supported\n", msg->method);
                }

                if(strcmp(msg->method,"set") == 0)
                {
                    bool ok = true;
                    body_value = body_JSON; //cJSON_GetObjectItem(body_JSON, "values");
                    if (body_value == NULL)
                    {
                        FPS_ERROR_PRINT("fims message body values key not found \n");
                        ok = false;
                    }
                    if (ok) 
                    {
                        if (body_value->type == cJSON_Array)
                        {
                            FPS_ERROR_PRINT("fims message body values should not be an array \n");
                            ok = false;
                        }
                    }
                    if(ok) 
                    {
                        //if(body_JSON->type == cJSON_String)  // maybe for CROB
                        if(body_JSON->type == cJSON_Number)
                        {
                            if (msg->nfrags > 2)
                            {
                                uri = msg->pfrags[2];
                                FPS_ERROR_PRINT("fims message frag 3 variable name [%s] \n", uri);
                                FPS_ERROR_PRINT("fims message frag 3 variable fvalue [%f] \n", body_JSON->valuedouble);
                                FPS_ERROR_PRINT("fims message frag 3 variable ivalue [%d] \n", body_JSON->valueint);
                                DbVar *db = sys_cfg.getDbVar(uri);
                                if (db != NULL)
                                {
                                    if ((db->type == Type_Analog) || (db->type == Type_Binary))
                                    {
                                        std::cout<< "Found variable type "<<db->type<<"\n";
                                        sys_cfg.setDbVar(uri, body_JSON);
                                        if (db->type == Type_Analog)
                                        {
                                            builder.Update(Analog(body_JSON->valuedouble), db->offset);
                                        }
                                        else if (db->type == Type_Binary)
                                        {
                                            builder.Update(Binary(body_JSON->valueint), db->offset);
                                        }
                                        outstation->Apply(builder.Build());
                                    }
                                    else 
                                    {
                                        std::cout << " Variable ["<<body_JSON->string<<"] variable ["<<uri<<"] wrong type or outstation\n";
                                    }
                                }
                                else 
                                {
                                    std::cout << " Variable ["<<body_JSON->string<<"] variable ["<<uri<<"] not found\n";
                                }
                                ok = false;
                            }
                        }
                    }
                    if (ok)
                    {
                        cJSON_ArrayForEach(iterator, body_value) 
                        {
                            std::cout << " Variable name ["<<iterator->string<<"]\n";
                            DbVar * db = sys_cfg.getDbVar(iterator->string);
                            if (db != NULL)
                            {
                                // outstation only allows binary and analog so far
                                std::cout<< "Found variable type "<<db->type<<"\n";
                                if (db->type == Type_Analog)
                                {
                                    builder.Update(Analog(iterator->valuedouble), db->offset);
                                    sys_cfg.setDbVar(iterator->string, iterator);
                                }
                                else if (db->type == Type_Binary)
                                {
                                    builder.Update(Binary(iterator->valueint), db->offset);
                                    sys_cfg.setDbVar(iterator->string, iterator);
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
