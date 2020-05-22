
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
//#include <asiodnp3/UpdateBuilder.h>

#include "newSOEHandler.h"
#include "newMasterApplication.h"

//#include <modbus/modbus.h>
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

void signal_handler (int sig)
{
    running = false;
    FPS_ERROR_PRINT("signal of type %d caught.\n", sig);
    signal(sig, SIG_DFL);
}



bool process_fims_message(fims_message* msg, server_data* server_map)
{

    char* uri = msg->uri;
    // uri was sent to this processes URI so it needs to handle it
    if(strncmp(uri, server_map->base_uri, strlen(server_map->base_uri)) == 0)
    {
        if(strcmp("set", msg->method) == 0)
        {
            uri = msg->pfrags[2];
            if(msg->nfrags > 2 && strncmp("reply", uri, strlen("reply")) == 0)
            {
                uri = msg->pfrags[3] - 1;
                uri_map::iterator uri_it = server_map->uri_to_register.find(uri);
                if(uri_it == server_map->uri_to_register.end())
                {
                    // failed to find in map
                    FPS_ERROR_PRINT( "Fims reply for unrequested uri: %s.\n", uri);
                    return false;
                }
                body_map* body = uri_it->second;
                cJSON* body_obj = cJSON_Parse(msg->body);
                //check for valid json
                if(body_obj == NULL)
                {
                    FPS_ERROR_PRINT(" Received invalid json object for set %s. \"%s\"\n", uri, msg->body);
                    return false;
                }
                for(body_map::iterator body_it = body->begin(); body_it != body->end(); ++body_it)
                {
                    cJSON* cur_obj = cJSON_GetObjectItem(body_obj, body_it->first);
                    if(cur_obj == NULL)
                    {
                        FPS_ERROR_PRINT("Failed to find %s in %s.\n", body_it->first, uri);
                        return false;
                    }
                    update_register_value(server_map->mb_mapping, body_it->second.first, body_it->second.second, cur_obj);
                }
            }
            else
            {
                if(msg->replyto != NULL)
                    server_map->p_fims->Send("set", msg->replyto, NULL, "Error: Method not implemented for that URI.");
                return true;
            }
        }
        else if(strcmp("get", msg->method) == 0)
        {
            //don't know when we would receive a get
            if(msg->replyto != NULL)
                server_map->p_fims->Send("set", msg->replyto, NULL, "Error: Method not implemented for that URI.");
            return true;
        }
        else
        {
            if(msg->replyto != NULL)
                server_map->p_fims->Send("set", msg->replyto, NULL, "Error: Method not implemented for that URI.");
            return true;
        }
    }
    else // Check to see if this is a publish we care about
    {
        if(strcmp("pub", msg->method) == 0)
        {
            uri_map::iterator uri_it = server_map->uri_to_register.find(msg->uri);
            if(uri_it == server_map->uri_to_register.end())
            {
                //failed to find uri in devices we care about.
                FPS_ERROR_PRINT("Received pub not in uri list: %s.\n", msg->uri);
                return false;
            }
            cJSON* body_obj = cJSON_Parse(msg->body);
            if(body_obj == NULL || body_obj->child == NULL)
            {
                if(body_obj != NULL)
                    cJSON_Delete(body_obj);
                return false;
            }
            for(cJSON* id_obj = body_obj->child; id_obj != NULL; id_obj = id_obj->next)
            {
                body_map::iterator body_it = uri_it->second->find(id_obj->string);
                if(body_it == uri_it->second->end())
                {
                    // Value not in our array ignore
                    continue;
                }
                update_register_value(server_map->mb_mapping, body_it->second.first, body_it->second.second, id_obj);
            }
            cJSON_Delete(body_obj);
        }
        else
            // Not our message so ignore
            return true;
    }
    return true;
}

// this subscribes to a number of uris which has been set up from reading the config file.
// TODO review this
bool initialize_map(server_data* server_map)
{
    if(server_map->p_fims->Subscribe((const char**)server_map->uris, server_map->num_uris + 1) == false)
    {
        FPS_ERROR_PRINT("Failed to subscribe to URI.\n");
        return false;
    }

    char replyto[256];
    for(uri_map::iterator it = server_map->uri_to_register.begin(); it != server_map->uri_to_register.end(); ++it)
    {
        snprintf(replyto, 256 , "%s/reply%s", server_map->base_uri, it->first);
        server_map->p_fims->Send("get", it->first, replyto, NULL);
        timespec current_time, timeout;
        clock_gettime(CLOCK_MONOTONIC, &timeout);
        timeout.tv_sec += 2;
        bool found = false;

        while(server_map->p_fims->Connected() && found != true &&
             (clock_gettime(CLOCK_MONOTONIC, &current_time) == 0) && (timeout.tv_sec > current_time.tv_sec ||
             (timeout.tv_sec == current_time.tv_sec && timeout.tv_nsec > current_time.tv_nsec + 1000)))
        {
            unsigned int us_timeout_left = (timeout.tv_sec - current_time.tv_sec) * 1000000 +
                    (timeout.tv_nsec - current_time.tv_nsec) / 1000;
            fims_message* msg = server_map->p_fims->Receive_Timeout(us_timeout_left);
            if(msg == NULL)
                continue;
            bool rc = process_fims_message(msg, server_map);
            if(strcmp(replyto, msg->uri) == 0)
            {
                found = true;
                if(rc == false)
                {
                    FPS_ERROR_PRINT("Error, failed to find value from config file in defined uri.\n");
                    server_map->p_fims->free_message(msg);
                    return false;
                }
            }
            server_map->p_fims->free_message(msg);
        }
        if(found == false)
        {
            FPS_ERROR_PRINT("Failed to find get response for %s.\n", it->first);
            return false;
        }
    }
    return true;
}


DNP3Manager* setupDNP3Manager(void)
{
    auto manager = new DNP3Manager(1, ConsoleLogger::Create());
    return manager;
}

// I think we can have several channels under one manager
std::shared_ptr<IChannel> setupDNP3channel(DNP3Manager* manager, const char* cname, const char* addr, int port)
{

     // Specify what log levels to use. NORMAL is warning and above
    // You can add all the comms logging by uncommenting below
    const uint32_t FILTERS = levels::NORMAL | levels::ALL_APP_COMMS;
    // This is the main point of interaction with the stack
    // Connect via a TCPClient socket to a outstation
    // repeat for each outstation

    // TODO setup our own PrintingChannelListener
    std::shared_ptr<IChannel> channel = manager->AddTCPClient(cname, FILTERS, ChannelRetry::Default(), {IPEndpoint(addr, port)},
                                        "0.0.0.0", PrintingChannelListener::Create());
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
                                     newSOEHandler::Create(ourDB), // callback for data processing  this generates the pub elements when we get data
                                     newMasterApplication::Create(ourDB), // master application instance this manages the collection of al the pub elements 
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
//
int addValueToCommand(cJSON *cj, sysCfg*cfgdb, CommandSet& commands, const char* valuestring, cJSON *cjvalue)
{
    // cjoffset must be a name
    // cjvalue may be an object

    if (valuestring == NULL)
    {
        FPS_ERROR_PRINT(" ************** %s offset is not  string\n",__FUNCTION__);
        return -1;
    }

    DbVar* pt = getDbVar(cfgdb, valuestring); 
    if (pt == NULL)
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

    if (pt->type == AnIn16) 
    {
        commands.Add<AnalogOutputInt16>({WithIndex(AnalogOutputInt16(cjvalue->valueint),pt->offset)});
        cfgdb->setDbVar(valuestring, cjvalue);
        addVarToCj(cfgdb, cj, valuestring);
    }
    else if (pt->type == AnIn32) 
    {
        commands.Add<AnalogOutputInt32>({WithIndex(AnalogOutputInt32(cjvalue->valueint),pt->offset)});
        cfgdb->setDbVar(valuestring, cjvalue);
        addVarToCj(cfgdb, cj, valuestring);

    }
    else if (pt->type == AnF32) 
    {
        commands.Add<AnalogOutputFloat32>({WithIndex(AnalogOutputFloat32(cjvalue->valuedouble),pt->offset)});
        cfgdb->setDbVar(valuestring, cjvalue);
        addVarToCj(cfgdb, cj, valuestring);

    }
    else if (pt->type == Type_Crob) 
    {
        FPS_DEBUG_PRINT(" ************* %s Var [%s] CROB setting value [%s]  to %d \n"
                                                    , __FUNCTION__
                                                    , pt->name.c_str()
                                                    , valuestring
                                                    , (int)StringToControlCode(valuestring)
                                                    );

        commands.Add<ControlRelayOutputBlock>({WithIndex(ControlRelayOutputBlock(StringToControlCode(valuestring)),pt->offset)});
        cfgdb->setDbVar(valuestring, cjvalue);
        addVarToCj(cfgdb, cj, valuestring);

    }
    else
    {
      FPS_ERROR_PRINT( " *************** %s Var [%s] not found in dbMap\n",__FUNCTION__, valuestring);  
      return -1;
    }
    return 0;   
}

int addValueToCommand(cJSON *cj, sysCfg*cfgdb, CommandSet& commands, cJSON *cjoffset, cJSON *cjvalue)
{
    return addValueToCommand(cj, cfgdb, commands, cjoffset->valuestring, cjvalue);
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

int addValueToVec(std::vector<DbVar*>&dbs, sysCfg*cfgdb, /*CommandSet& commands,*/ cJSON *cjoffset, cJSON *cjvalue)
{
    return addValueToVec(dbs, cfgdb, /*commands,*/ cjoffset->valuestring, cjvalue);
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
    // 
    // set /components/master/dnp3_outstation '{"type":"xx", offset:yy value: zz}'
    // set /components/master/dnp3_outstation '{"CROB":[{"offset":4,"value":"LATCH_ON"}]}'

    // set /components/master/dnp3_outstation '{"name1":1, "name2":}'
    // or 
    // set /components/master/dnp3_outstation '{"name1":{"value":1}, "name2":{"value":2}'}
    
    if(ok)
    {
        // does the master support incoming pubs ??
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
            //cJSON* cj = cJSON_CreateObject();

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

            // if(msg->replyto != NULL)
            // {
            //     const char* reply = cJSON_PrintUnformatted(cj);
            //     FPS_DEBUG_PRINT("Got reply  [%s] replyto [%s] \n", reply, msg->replyto); 
            //     p_fims->Send("set", msg->replyto, NULL, reply);
            //     free((void* )reply);
            // }
            // cJSON_Delete(cj);
        
            ok = false;  // we are done
            return body_JSON;
            //return dbs.size();
        }

        if (ok) 
        {
            if(strcmp(msg->method,"set") == 0)
            {
                //cJSON* cj = cJSON_CreateObject(); // used for reply

                //    case AnIn16:
                //         return "AnOPInt16";
                //     case AnIn32:
                //         return "AnOPInt32";
                //     case AnF32:
                //         return "AnOPF32";
                //     case Type_Crob:
                //         return "CROB";
                //     case Type_Analog:
                //         return "analog";
                //     case Type_Binary:
                //         return "binary";
                //     default:
                //         return "Unknwn";

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
                                //commands.Add<ControlRelayOutputBlock>({WithIndex(ControlRelayOutputBlock(ControlCodeFromType(cval)), db->offset)});
                                
                                sys->setDbVarIx(Type_Crob, db->offset, cval);
                                dbs.push_back(db);
                                // send the response
                                //addVarToCj(cj, db);

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
                    // const char* reply = cJSON_PrintUnformatted(cj);
                    // cJSON_Delete(cj);
                    // if(msg->replyto != NULL)
                    //     p_fims->Send("set", msg->replyto, NULL, reply);
                    // free((void* )reply);
                    // ok = false;  // we are done
                    return body_JSON;//return dbs.size();
                }
                // scan the development options  -- these do not get replies
                itypeA16 = cJSON_GetObjectItem(body_JSON, "AnOPInt16");
                itypeA32 = cJSON_GetObjectItem(body_JSON, "AnOPInt32");
                itypeF32 = cJSON_GetObjectItem(body_JSON, "AnOPF32");
                itypeCROB = cJSON_GetObjectItem(body_JSON, "CROB");
                //the value list does get a reply
                itypeValues = body_JSON;//cJSON_GetObjectItem(body_JSON, "values");

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
                            //addValueToCommand(cj, &sys_cfg, commands, cjoffset, cjvalue);
                            addValueToVec(dbs, sys, /*commands,*/ cjoffset, cjvalue);
                            //addVarToCj(cj, db); included in the above

                            //commands.Add<AnalogOutputInt16>({WithIndex(AnalogOutputInt16(cjvalue->valueint),dboffset)});
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
                            // FPS_DEBUG_PRINT("Found variable name  [%s] child type %d next %p\n"
                            //                         , iterator->string
                            //                         , iterator->child->type
                            //                         , (void*)iterator->next
                            //);  
                            //addValueToCommand(cj, &sys_cfg, commands, iterator->string, iterator);
                            addValueToVec(dbs, sys, /*commands,*/ iterator->string, iterator);
                            iterator = iterator->next;
                        }
                        FPS_DEBUG_PRINT("***** Done with variable list \n\n");
                    }
                    // const char* reply = cJSON_PrintUnformatted(cj);
                    // cJSON_Delete(cj);
                    // cj = NULL;

                    // if((msg->replyto != NULL) && (strlen(reply) > 2))
                    //     p_fims->Send("set", msg->replyto, NULL, reply);
                    // free((void* )reply);
                    // ok = false;  // we are done
                    return body_JSON;//return dbs.size();
                }
                if (itypeA16 != NULL)
                {
                    // decode A16
                    if (cJSON_IsArray(itypeA16)) 
                    {
                        cJSON_ArrayForEach(iterator, itypeA16) 
                        {
                            cjoffset = cJSON_GetObjectItem(iterator, "offset");
                            cjvalue = cJSON_GetObjectItem(iterator, "value");
                            int dboffset = cjoffset->valueint;
                            FPS_DEBUG_PRINT(" *****Adding Direct AnOPInt16 value %d offset %d \n", cjvalue->valueint, dboffset);
                            //commands.Add<AnalogOutputInt16>({WithIndex(AnalogOutputInt16(cjvalue->valueint), dboffset)});
                            // TODO fix this
                            sys->setDbVarIx(AnIn16, dboffset, cjvalue->valueint);
                            DbVar* db = sys->getDbVar(cjoffset->valuestring);
                            if(db)
                            {
                                dbs.push_back(db);
                            }
                        }
                    }
                }
                if (itypeA32 != NULL)
                {
                    // decode A32
                    if (cJSON_IsArray(itypeA32)) 
                    {
                        cJSON_ArrayForEach(iterator, itypeA32) 
                        {
                            cjoffset = cJSON_GetObjectItem(iterator, "offset");
                            cjvalue = cJSON_GetObjectItem(iterator, "value");
                            int dboffset = cjoffset->valueint;
                            //commands.Add<AnalogOutputInt32>({WithIndex(AnalogOutputInt32(cjvalue->valueint),dboffset)});
                            sys->setDbVarIx(AnIn32, dboffset, cjvalue->valueint);
                            // TODO fix this
                            //sys->setDbVarIx(AnIn16, dboffset, cjvalue->valueint);
                            DbVar* db = sys->getDbVar(cjoffset->valuestring);
                            if(db)
                            {
                                dbs.push_back(db);
                            }
                        }
                    }
                }
                if (itypeF32 != NULL)
                {
                    // decode F32
                    if (cJSON_IsArray(itypeF32)) 
                    {
                        cJSON_ArrayForEach(iterator, itypeF32) 
                        {
                            cjoffset = cJSON_GetObjectItem(iterator, "offset");
                            cjvalue = cJSON_GetObjectItem(iterator, "value");
                            int dboffset = cjoffset->valueint;
                            //commands.Add<AnalogOutputFloat32>({WithIndex(AnalogOutputFloat32(cjvalue->valuedouble),dboffset)});
                            sys->setDbVarIx(AnF32, dboffset, cjvalue->valuedouble);
                            // TODO fix this
                            //sys->setDbVarIx(AnIn16, dboffset, cjvalue->valueint);
                            DbVar* db = sys->getDbVar(cjoffset->valuestring);
                            if(db)
                            {
                                dbs.push_back(db);
                            }
                        }
                    }
                }

                // LATCH OFF
                //C8 05 0C 01 28 01 00 15 00 04 01 64 00 00 00 64 00 00 00 00
                // LATCH_ON
                //CC 05 0C 01 28 01 00 15 00 03 01 64 00 00 00 64 00 00 00 00

                if (itypeCROB != NULL)
                {
                    // decode CROB
                    if (cJSON_IsArray(itypeCROB)) 
                    {
                        cJSON_ArrayForEach(iterator, itypeCROB) 
                        {
                            cjoffset = cJSON_GetObjectItem(iterator, "offset");
                            cjvalue = cJSON_GetObjectItem(iterator, "value");
                            int dboffset = cjoffset->valueint;
                            uint8_t cval2 = ControlCodeToType(StringToControlCode(cjvalue->valuestring));
                            //commands.Add<ControlRelayOutputBlock>({WithIndex(ControlRelayOutputBlock(ControlCodeFromType(cval2)), dboffset)});
                            //TODO 
                            sys->setDbVarIx(Type_Crob, dboffset, cval2);
                            FPS_DEBUG_PRINT(" ***** %s Adding Direct CROB value %s offset %d uint8 cval2 0x%02x\n"
                                        , __FUNCTION__, cjvalue->valuestring, dboffset
                                            //, cval  //ControlCodeToType(StringToControlCode(cjvalue->valuestring))
                                            , cval2
                                            );
                            DbVar* db = sys->getDbVar(cjoffset->valuestring);
                            if(db)
                            {
                                dbs.push_back(db);
                            }

                        }
                    }
                }
            }
        }
    }
    return body_JSON;//return dbs.size();
}

void addVarToCommands (CommandSet & commands, DbVar *db)
{
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
    char sub[] = "/components";
    char* subs = sub;
    //char* uri;
    bool publish_only = false;
    bool running = true;
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    server_data *server_map = NULL;
    //int header_length,;
    //int serial_fd;
    int fims_socket;
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
    sys_cfg.p_fims = p_fims = new fims();

    auto manager = setupDNP3Manager();
    if (!manager){
        FPS_ERROR_PRINT("Error in setupDNP3Manager.\n");
        return 1;
    }
    // now we use data from the config file
    //std::shared_ptr<IChannel> 
    //auto channel = setupDNP3channel(manager, "tcpclient1", "127.0.0.1", 20001);
    auto channel = setupDNP3channel(manager, sys_cfg.id, sys_cfg.ip_address, sys_cfg.port);
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

    FPS_DEBUG_PRINT("Map configured: Initializing data.\n");
    
    // subs = /components
    if(p_fims->Subscribe((const char**)&subs, 1, (bool *)&publish_only) == false)
    {
        FPS_ERROR_PRINT("Subscription failed.\n");
        p_fims->Close();
        return 1;
    }

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
        fims_message* msg = p_fims->Receive();
        if(msg != NULL)
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
                    DbVar* db = dbp.first;
                    addVarToCommands (commands, db);
                    addVarToCj(cj, db);
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
       
    if (0)
    {
        fd_set connections_with_data;
        fims_socket = sys_cfg.p_fims->get_socket();

        if(fims_socket != -1)
            FD_SET(fims_socket, &all_connections);
        else
        {
            FPS_ERROR_PRINT("Failed to get fims socket.\n");
            rc = 1;
            goto cleanup;
        }

        fd_max = (fd_max > fims_socket) ? fd_max: fims_socket;
        FPS_DEBUG_PRINT("Fims Setup complete: now for DNP3\n");

        while(running)
        {
            connections_with_data = all_connections;
            // Select will block until one of the file descriptors has data
            if(-1 == select(fd_max+1, &connections_with_data, NULL, NULL, NULL))
            {
                FPS_ERROR_PRINT("server select() failure: %s.\n", strerror(errno));
                break;
            }
            //Loop through file descriptors to see which have data to read
            for(int current_fd = 0; current_fd <= fd_max; current_fd++)
            {
                // if no data on this file descriptor skip to the next one
                if(!FD_ISSET(current_fd, &connections_with_data))
                    continue;
                if(current_fd == fims_socket)
                {
                    // Fims message received
                    fims_message* msg = server_map->p_fims->Receive_Timeout(1*MICROSECOND_TO_MILLISECOND);
                    if(msg == NULL)
                    {
                        if(server_map->p_fims->Connected() == false)
                        {
                            // fims connection closed
                            FPS_ERROR_PRINT("Fims connection closed.\n");
                            FD_CLR(current_fd, &all_connections);
                            break;
                        }
                        else
                            FPS_ERROR_PRINT("No fims message. Select led us to a bad place.\n");
                    }
                    else
                    {
                        process_fims_message(msg, server_map);
                        server_map->p_fims->free_message(msg);
                    }
                }
            }
        }

        FPS_DEBUG_PRINT("Main loop complete: Entering clean up.\n");
    }

    cleanup:
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


