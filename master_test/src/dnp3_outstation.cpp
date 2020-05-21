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
    config.link.LocalAddr = localAddr;
    config.link.RemoteAddr = remoteAddr;

    config.link.KeepAliveTimeout = openpal::TimeDuration::Max();

    // You can optionally change the default reporting variations or class assignment prior to enabling the outstation
    ConfigureDatabase(config.dbConfig);

    // Create a new outstation with a log level, command handler, and
    // config info this	returns a thread-safe interface used for
    // updating the outstation's database.
    auto outstation = channel->AddOutstation("outstation", SuccessCommandHandler::Create(),
                                             DefaultOutstationApplication::Create(), config);

    // Enable the outstation and start communications
    outstation->Enable();
    return outstation;
}

int addValueToVec(std::vector<DbVar*>&dbs, sysCfg*sys, /*CommandSet& commands,*/ const char* valuestring, cJSON *cjvalue)
{
    // cjoffset must be a name
    // cjvalue may be an object

    if (valuestring == NULL)
    {
        FPS_ERROR_PRINT(" ************** %s offset is not  string\n",__FUNCTION__);
        return -1;
    }

    DbVar* db = getDbVar(sys, valuestring); 
    if (db == NULL)
    {
        FPS_ERROR_PRINT( " ************* %s Var [%s] not found in dbMap\n", __FUNCTION__, valuestring);
        return -1;
    }
    
    FPS_DEBUG_PRINT(" ************* %s Var [%s] found in dbMap\n", __FUNCTION__, valuestring);

    if (cjvalue->type == cJSON_Object)
    {
        cjvalue = cJSON_GetObjectItem(cjvalue, "value");
    }

    if (!cjvalue)
    {
        FPS_ERROR_PRINT(" ************** %s value not correct\n",__FUNCTION__);
        return -1;
    }

    if (db->type == AnIn16) 
    {
        //commands.Add<AnalogOutputInt16>({WithIndex(AnalogOutputInt16(cjvalue->valueint), db->offset)});
        sys->setDbVar(valuestring, cjvalue);
        dbs.push_back(db);
        //addVarToCj(cfgdb, cj, valuestring);
    }
    else if (db->type == AnIn32) 
    {
        //commands.Add<AnalogOutputInt32>({WithIndex(AnalogOutputInt32(cjvalue->valueint),pt->offset)});
        sys->setDbVar(valuestring, cjvalue);
        //addVarToCj(cfgdb, cj, valuestring);
        dbs.push_back(db);

    }
    else if (db->type == AnF32) 
    {
        //commands.Add<AnalogOutputFloat32>({WithIndex(AnalogOutputFloat32(cjvalue->valuedouble),pt->offset)});
        sys->setDbVar(valuestring, cjvalue);
        dbs.push_back(db);
        //addVarToCj(cfgdb, cj, valuestring);
    }

    else if (db->type == Type_Crob) 
    {
        FPS_DEBUG_PRINT(" ************* %s Var [%s] CROB setting value [%s]  to %d \n"
                                                    , __FUNCTION__
                                                    , db->name.c_str()
                                                    , valuestring
                                                    , (int)StringToControlCode(valuestring)
                                                    );

        //commands.Add<ControlRelayOutputBlock>({WithIndex(ControlRelayOutputBlock(StringToControlCode(valuestring)),pt->offset)});
        sys->setDbVar(valuestring, cjvalue);
        //addVarToCj(cfgdb, cj, valuestring);
        dbs.push_back(db);
    }
    else
    {
      FPS_ERROR_PRINT( " *************** %s Var [%s] not found in dbMap\n",__FUNCTION__, valuestring);  
      return -1;
    }
    return dbs.size();   
}

void addVarToBuilder (UpdateBuilder& builder, DbVar *db)
{
    switch (db->type) 
    {
        case Type_Crob:
        {
            builder.Update(Binary(db->crob), db->offset);
            break;
        }
        case AnIn16:
        {
            builder.Update(Analog(db->anInt16), db->offset);
            break;
        }
        case AnIn32:
        {
            builder.Update(Analog(db->anInt32), db->offset);
            break;
        }
        case AnF32:
        {
            builder.Update(Analog(db->anF32), db->offset);
            break;
        }
        default:
            break;
    }
}

cJSON* parseTheThing( std::vector<DbVar*>&dbs, sysCfg*sys, fims_message*msg, const char* who)
{
    const char* uri = NULL;
    int fragptr = 1;
    bool ok = true;
    cJSON* body_JSON = cJSON_Parse(msg->body);
    cJSON* itypeA16 = NULL;
    cJSON* itypeA32 = NULL;
    cJSON* itypeF32 = NULL;
    cJSON* itypeValues = NULL;
    cJSON* itypeCROB = NULL;
    cJSON* cjoffset = NULL;
    cJSON* cjvalue = NULL;
    cJSON* iterator = NULL;
    //CommandSet commands;
    
    if (body_JSON == NULL)
    {
        if(strcmp(msg->method,"set") == 0)
        {
            FPS_ERROR_PRINT("fims message body is NULL or incorrectly formatted for  (%s) \n", msg->method);
            ok = false;
            return NULL;
        }
    }
    
    if (msg->nfrags < 2)
    {
        FPS_ERROR_PRINT("fims message not enough pfrags id [%s] \n", sys->id);
        ok = false;
        return body_JSON;
    }

    if(ok)
    {
        uri = msg->pfrags[fragptr];
        if (strncmp(uri, who, strlen(who)) != 0)
        {
            FPS_ERROR_PRINT("fims message frag 1 [%s] not for   [%s] \n", uri, who);
            ok = false; // temp we neeed the master frag 
            return body_JSON;
        }
        else
        {
            fragptr = 1;
        }              
    }

    if(ok)
    {
        ok = false;
        uri = msg->pfrags[fragptr+1];
        cout<<"uri: "<<uri<<endl;
        if (strncmp(uri, sys->id, strlen(sys->id)) == 0)
        {
            ok = true;
        }
        else
        {
            FPS_ERROR_PRINT("fims message frag %d [%s] not for this master [%s] \n", fragptr+1, uri, sys->id);
             return body_JSON;

        }
    }
    // set /components/master/dnp3_outstation '{"name1":1, "name2":}'
    // or 
    // set /components/master/dnp3_outstation '{"name1":{"value":1}, "name2":{"value":2}'}
    
    if(ok)
    {
        if((strcmp(msg->method,"set") == 0) || (strcmp(msg->method,"get") == 0))
        {
            ok = true;
        }
        else
        {
            FPS_ERROR_PRINT("fims unsupported method [%s] \n", msg->method);
            ok = false;
            return body_JSON;
        }
        //-m set -u "/dnp3/<some_master_id>/<some_outstation_id> '{"AnalogInt32": [{"offset":"name_or_index","value":52},{"offset":2,"value":5}]}' 
        //-m set -u "/components/<some_master_id>/[<some_outstation_id>] '{"AnalogInt32": [{"offset":"name_or_index","value":52},{"offset":2,"value":5}]}' 
    }

    if(ok)
    {
        // get is OK codewise..
        if(strcmp(msg->method,"get") == 0)
        {
            FPS_ERROR_PRINT("fims method [%s] almost  supported for master\n", msg->method);

            // is this a singleton
            if ((int)msg->nfrags > fragptr+2)
            {
                uri = msg->pfrags[fragptr+2];  // TODO check for delim. //components/master/dnp3_outstation/line_voltage/stuff
                FPS_DEBUG_PRINT("fims message frag %d variable name [%s] \n", fragptr+2,  uri);
                DbVar* db = sys->getDbVar(uri);
                if (db != NULL)
                {
                    FPS_DEBUG_PRINT("Found variable [%s] type  %d \n", db->name.c_str(), db->type); 
                    //addVarToCj(cj, db);
                    dbs.push_back(db);
                    return body_JSON;
                }
            }
            // else get them all
            else
            {
                sys->addVarsToVec(dbs);
                return body_JSON;
            }
        
            ok = false;  // we are done
            return body_JSON;
            //return dbs.size();
        }

        if (ok) 
        {
            if(strcmp(msg->method,"set") == 0)
            {
                // handle a single item set  crappy code for now, we'll get a better plan in a day or so 
                if ((int)msg->nfrags > fragptr+2)
                {
                    uri = msg->pfrags[fragptr+2];  // TODO check for delim. //components/master/dnp3_outstation/line_voltage/stuff
                    FPS_DEBUG_PRINT("fims message frag %d variable name [%s] \n", fragptr+2,  uri);
                    DbVar* db = sys->getDbVar(uri);
                    if (db != NULL)
                    {
                        FPS_DEBUG_PRINT("Found variable type  %d \n", db->type);
                        itypeValues = body_JSON;
                        // allow '"string"' OR '{"value":"string"}'
                        if(itypeValues->type == cJSON_Object)
                        {
                            itypeValues = cJSON_GetObjectItem(itypeValues, "value");
                        }
                        // Only Crob gets a string 
                        if(itypeValues && (itypeValues->type == cJSON_String))
                        {
                            if(db->type == Type_Crob)
                            {

                                uint8_t cval = ControlCodeToType(StringToControlCode(itypeValues->valuestring));
                                
                                sys->setDbVarIx(Type_Crob, db->offset, cval);
                                dbs.push_back(db);
                                // send the response
                                FPS_DEBUG_PRINT(" ***** %s Adding Direct CROB value %s offset %d uint8 cval2 0x%02x\n"
                                                , __FUNCTION__, itypeValues->valuestring, db->offset
                                                , cval
                                                );
                            }
                            // TODO any other strings
                            // do we have to convert strings into numbers ??
                        }
                        // stop this being used 
                        itypeValues = NULL;
                    }
                    return body_JSON;//return dbs.size();
                }
                // scan the development options  -- these do not get replies
                itypeA16 = cJSON_GetObjectItem(body_JSON, "AnOPInt16");
                itypeA32 = cJSON_GetObjectItem(body_JSON, "AnOPInt32");
                itypeF32 = cJSON_GetObjectItem(body_JSON, "AnOPF32");
                itypeCROB = cJSON_GetObjectItem(body_JSON, "CROB");
                //the value list does get a reply
                itypeValues = body_JSON;

                // process {"valuex":xxx,"valuey":yyy} ; xxx or yyy could be a number or {"value":val}
                if ((itypeA16 == NULL) && (itypeA32 == NULL) && (itypeF32 == NULL) && (itypeCROB == NULL)) 
                {
                    // decode values may be in an array 
                    if (cJSON_IsArray(itypeValues)) 
                    {
                        cJSON_ArrayForEach(iterator, itypeValues) 
                        {
                            cjoffset = cJSON_GetObjectItem(iterator, "offset");
                            cjvalue = cJSON_GetObjectItem(iterator, "value");
                            addValueToVec(dbs, sys, /*commands,*/ cjoffset->valuestring, cjvalue);
                        }
                    }
                    else
                    {
                        // process a simple list
                        iterator = itypeValues->child;
                        FPS_DEBUG_PRINT("****** Start with variable list iterator->type %d\n\n", iterator->type);

                        while(iterator!= NULL)
                        {   
                            FPS_DEBUG_PRINT("Found variable name  [%s] child %p \n"
                                                    , iterator->string
                                                    , (void *)iterator->child
                                                    );
                            addValueToVec(dbs, sys, iterator->string, iterator);
                            iterator = iterator->next;
                        }
                        FPS_DEBUG_PRINT("***** Done with variable list \n\n");
                    }
                    return body_JSON;//return dbs.size();
                }
            }
        }
    }
    return body_JSON;//return dbs.size();
}

sysCfg * xcfgdb;

int main(int argc, char* argv[])
{
    sysCfg sys_cfg;
    xcfgdb = &sys_cfg;
    int rc = 0;
    int fims_connect = 0;
    p_fims = new fims();
    char sub[] = "/components";
    char* subs = sub;
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
    // subs = /components
    if(p_fims->Subscribe((const char**)&subs, 1, (bool *)&publish_only) == false)
    {
        FPS_ERROR_PRINT("Subscription failed.\n");
        p_fims->Close();
        return 1;
    }

    while(running && p_fims->Connected())
    {
        fims_message* msg = p_fims->Receive();
        if(msg != NULL)
        {
            std::vector<DbVar*>dbs; // collect all the parsed vars here
            cJSON* cjb = parseTheThing(dbs, &sys_cfg, msg, "outstation");
            if(dbs.size() > 0)
            {
                cJSON* cj = NULL;                
                if(msg->replyto != NULL)
                    cj = cJSON_CreateObject();
                    
                UpdateBuilder builder;
                while (!dbs.empty())
                {
                    DbVar* db = dbs.back();
                    addVarToBuilder(builder, db);
                    addVarToCj(cj, db);
                    dbs.pop_back();
                }

                //finalize set of updates
                auto updates = builder.Build();
                outstation->Apply(updates);

                if(cj)
                {
                    const char* reply = cJSON_PrintUnformatted(cj);
                    cJSON_Delete(cj);
                    cj = NULL;
                    p_fims->Send("set", msg->replyto, NULL, reply);
                    free((void* )reply);
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