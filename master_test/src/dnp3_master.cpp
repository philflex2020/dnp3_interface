
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
    fprintf(stderr, "signal of type %d caught.\n", sig);
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
                    fprintf(stderr, "Fims reply for unrequested uri: %s.\n", uri);
                    return false;
                }
                body_map* body = uri_it->second;
                cJSON* body_obj = cJSON_Parse(msg->body);
                //check for valid json
                if(body_obj == NULL)
                {
                    fprintf(stderr, "Received invalid json object for set %s. \"%s\"\n", uri, msg->body);
                    return false;
                }
                for(body_map::iterator body_it = body->begin(); body_it != body->end(); ++body_it)
                {
                    cJSON* cur_obj = cJSON_GetObjectItem(body_obj, body_it->first);
                    if(cur_obj == NULL)
                    {
                        fprintf(stderr, "Failed to find %s in %s.\n", body_it->first, uri);
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
                fprintf(stderr, "Received pub not in uri list: %s.\n", msg->uri);
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
        fprintf(stderr, "Failed to subscribe to URI.\n");
        return false;
    }

    char replyto[256];
    for(uri_map::iterator it = server_map->uri_to_register.begin(); it != server_map->uri_to_register.end(); ++it)
    {
        sprintf(replyto, "%s/reply%s", server_map->base_uri, it->first);
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
                    fprintf(stderr, "Error, failed to find value from config file in defined uri.\n");
                    server_map->p_fims->free_message(msg);
                    return false;
                }
            }
            server_map->p_fims->free_message(msg);
        }
        if(found == false)
        {
            fprintf(stderr, "Failed to find get response for %s.\n", it->first);
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
// TODO CROB
int addValueToCommand(sysCfg*cfgdb, CommandSet& commands, cJSON *cjoffset, cJSON *cjvalue)
{
    // cjoffset must be a name
    if (!cjoffset->valuestring)
    {
        fprintf(stderr, " ************** %s offset is not  string\n",__FUNCTION__);
        return -1;
    }

    DbVar* pt = getDbVar(cfgdb, cjoffset->valuestring); 
    if (pt == NULL)
    {
        fprintf(stderr, " ************* %s Var [%s] not found in dbMap\n", __FUNCTION__, cjoffset->valuestring);
        return -1;
    }

    fprintf(stderr, " ************* %s Var [%s] found in dbMap\n", __FUNCTION__, cjoffset->valuestring);

    if (pt->type == AnIn16) 
    {
        commands.Add<AnalogOutputInt16>({WithIndex(AnalogOutputInt16(cjvalue->valueint),pt->offset)});
        cfgdb->setDbVar(cjoffset->valuestring, cjvalue);
        //pt->valueint = cjvalue->valueint;
        //pt->anInt16 = cjvalue->valueint;
        
    }
    else if (pt->type == AnIn32) 
    {
        commands.Add<AnalogOutputInt32>({WithIndex(AnalogOutputInt32(cjvalue->valueint),pt->offset)});
        cfgdb->setDbVar(cjoffset->valuestring, cjvalue);
        //pt->valueint = cjvalue->valueint;
        //pt->anInt32 = cjvalue->valueint;
    }
    else if (pt->type == AnF32) 
    {
        commands.Add<AnalogOutputFloat32>({WithIndex(AnalogOutputFloat32(cjvalue->valuedouble),pt->offset)});
        cfgdb->setDbVar(cjoffset->valuestring, cjvalue);
        //pt->anF32 = cjvalue->valuedouble;
    }
    else if (pt->type == Type_Crob) 
    {
        fprintf(stderr, " ************* %s Var [%s] CROB setting value [%s]  to %d \n"
                                                    , __FUNCTION__
                                                    , pt->name,c_str()
                                                    , cjvalue->valuestring
                                                    , (int)StringToControlCode(cjvalue->valuestring)
                                                    );

        //set { "myCROB":"LATCH_ON", ...}
        // decode somevalue to ControlCode 
        commands.Add<ControlRelayOutputBlock>({WithIndex(ControlRelayOutputBlock(StringToControlCode(cjvalue->valuestring)),pt->offset)});
        cfgdb->setDbVar(cjoffset->valuestring, cjvalue);

    }
    else
    {
      fprintf(stderr, " *************** %s Var [%s] not found in dbMap\n",__FUNCTION__, cjoffset->valuestring);  
      return -1;
    }
    return 0;   
}

int main(int argc, char *argv[])
{
    fims* p_fims;
    char sub[] = "/components";
    char* subs = sub;
    char* uri;
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

    fprintf(stderr, "Reading config file and starting setup.\n");
    cJSON* config = get_config_json(argc, argv);
    if(config == NULL)
    {
        fprintf(stderr, "Error reading config file\n");
        return 1;
    }
    if(!parse_system(config, &sys_cfg)) 
    {
        fprintf(stderr, "Error reading system from config file.\n");
        cJSON_Delete(config);
        return 1;
    }
    if(!parse_variables(config, &sys_cfg)) 
    {
        fprintf(stderr, "Error reading variabled from config file.\n");
        cJSON_Delete(config);
        return 1;
    }

    cJSON_Delete(config);
    sys_cfg.p_fims = p_fims = new fims();

    auto manager = setupDNP3Manager();
    if (!manager){
        fprintf(stderr, "Error in setupDNP3Manager.\n");
        return 1;
    }
    // now we use data from the config file
    //std::shared_ptr<IChannel> 
    //auto channel = setupDNP3channel(manager, "tcpclient1", "127.0.0.1", 20001);
    auto channel = setupDNP3channel(manager, sys_cfg.id, sys_cfg.ip_address, sys_cfg.port);
    if (!channel){
        fprintf(stderr, "Error in setupDNP3channel.\n");
        delete manager;
        return 1;
    }
        //std::shared_ptr<IChannel> 
    auto master = setupDNP3master (channel, "master1", &sys_cfg , sys_cfg.local_address, sys_cfg.remote_address /*RemoteAddr*/);
    if (!master){
        fprintf(stderr, "Error in setupDNP3master.\n");
        delete manager;
        return 1;
    }
    //we cant do this
    //auto master2 = setupDNP3master (channel2, "master2", ourDB , 2 /*localAddr*/ , 10 /*RemoteAddr*/);
    //if (!master2){
    //    printf("fooey 4\n");
    //}

    fprintf(stderr, "DNP3 Setup complete: Entering main loop.\n");

    //sys_cfg.p_fims = new fims();

    if (p_fims == NULL)
    {
        fprintf(stderr,"Failed to allocate connection to FIMS server.\n");
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
        fprintf(stderr,"Failed to establish connection to FIMS server.\n");
        rc = 1;
        goto cleanup;
    }

    fprintf(stderr, "Map configured: Initializing data.\n");
    //todo this should be defined to a better length
    //char uri[100];
    //sprintf(uri,"/interfaces/%s", sys_cfg.name);
    //if(p_fims->Connect((char *)"fims_master2") == false)
    //{
    //    printf("Connect failed.\n");
    //    p_fims->Close();
    //    return 1;
    //}
    // subs = /components
    if(p_fims->Subscribe((const char**)&subs, 1, (bool *)&publish_only) == false)
    {
        printf("Subscription failed.\n");
        p_fims->Close();
        return 1;
    }
    //build/release/fims_send -m set -u "/dnp3/master" '{"type":"analog32","offset":4,"value":38}'
    //fims_send -m set -u "/components/<some_master_id>/<some_outstation_id> '{"AnalogInt32": [{"offset":"name_or_index","value":52},{"offset":2,"value":5}]}' 
    // AnalogOutputInt16 ao(100);
    // AnalogOutputInt16 a1(101);
    // AnalogOutputInt16 a2(102);
    // AnalogOutputInt16 a3(103);
    //         master->DirectOperate(CommandSet(
    //             {WithIndex(ao, 2),
    //             WithIndex(a1, 3),
    //             WithIndex(a2, 4),
    //             WithIndex(a3, 5)}
    //             ), PrintingCommandCallback::Get());
    //         master->SelectAndOperate(CommandSet(
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
            
            if (body_JSON == NULL)
            {
                FPS_ERROR_PRINT("fims message body is NULL or incorrectly formatted: (%s) \n", msg->body);
                ok = false;
            }
            
            if (msg->nfrags < 3)
            {
                FPS_ERROR_PRINT("fims message not enough pfrags outstation [%s] \n", sys_cfg.id);
                ok = false;
            }
            if(ok)
            {
                uri = msg->pfrags[1];
                if (strncmp(uri,"master",strlen("master")) != 0)
                {
                    FPS_ERROR_PRINT("fims message frag 1 [%s] not for  master [%s] \n", uri, "master");
                    ok = false;
                }
            }
            if(ok)
            {
                uri = msg->pfrags[2];
                if (strncmp(uri,sys_cfg.id,strlen(sys_cfg.id)) != 0)
                {
                    FPS_ERROR_PRINT("fims message frag 2 [%s] not for this master [%s] \n", uri, sys_cfg.id);
                    ok = false;
                }
            }
            // 
            // set /components/master/dnp3_outstation '{"type":"xx", offset:yy value: zz}'
            // set /components/master/dnp3_outstation '{"type":"analog", "offset":01, "value": 2.34}'
            // set /components/master/dnp3_outstation '{"values":{"name1":value1, "name2":value2}}'
            ok = false;
            if(strcmp(msg->method,"set") == 0)
            {
                ok = true;
            }
            if(strcmp(msg->method,"get") == 0)
            {
                ok = true;
            }
            if(ok == false)
            {
                FPS_ERROR_PRINT("fims unsupported method [%s] \n", msg->method);
            }
            //-m set -u "/dnp3/<some_master_id>/<some_outstation_id> '{"AnalogInt32": [{"offset":"name_or_index","value":52},{"offset":2,"value":5}]}' 
            //-m set -u "/components/<some_master_id>/[<some_outstation_id>] '{"AnalogInt32": [{"offset":"name_or_index","value":52},{"offset":2,"value":5}]}' 

            if(strcmp(msg->method,"get") == 0)
            {
                FPS_ERROR_PRINT("fims method [%s] almost  supported for master\n", msg->method);
                cJSON* cj = cJSON_CreateObject();

                if (msg->nfrags > 3)
                {
                    uri = msg->pfrags[3];  // TODO check for delim. //components/master/dnp3_outstation/line_voltage/stuff
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
                ok = false;  // we are done
            }

            if (ok) 
            {
                if(strcmp(msg->method,"set") == 0)
                {
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
                    itypeA16 = cJSON_GetObjectItem(body_JSON, "AnOPInt16");
                    itypeA32 = cJSON_GetObjectItem(body_JSON, "AnOPInt32");
                    itypeF32 = cJSON_GetObjectItem(body_JSON, "AnOPF32");
                    itypeCROB = cJSON_GetObjectItem(body_JSON, "CROB");
                    itypeValues = body_JSON;//cJSON_GetObjectItem(body_JSON, "values");

                    CommandSet commands;
                    if ((itypeA16 == NULL) && (itypeA32 == NULL) && (itypeF32 == NULL) && (itypeCROB == NULL)) 
                    {
                        // decode values must be in an array //TODO single option
                        if (cJSON_IsArray(itypeValues)) 
                        {
                            cJSON_ArrayForEach(iterator, itypeValues) 
                            {
                                cjoffset = cJSON_GetObjectItem(iterator, "offset");
                                cjvalue = cJSON_GetObjectItem(iterator, "value");
                                addValueToCommand(&sys_cfg, commands, cjoffset, cjvalue);
                                //commands.Add<AnalogOutputInt16>({WithIndex(AnalogOutputInt16(cjvalue->valueint),dboffset)});
                            }
                        }
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
                                printf(" *****Adding Direct AnOPInt16 value %d offset %d \n", cjvalue->valueint, dboffset);
                                commands.Add<AnalogOutputInt16>({WithIndex(AnalogOutputInt16(cjvalue->valueint), dboffset)});
                                sys_cfg.setDbVarIx(AnIn16, dboffset, cjvalue->valueint);
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
                                commands.Add<AnalogOutputInt32>({WithIndex(AnalogOutputInt32(cjvalue->valueint),dboffset)});
                                sys_cfg.setDbVarIx(AnIn32, dboffset, cjvalue->valueint);
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
                                commands.Add<AnalogOutputFloat32>({WithIndex(AnalogOutputFloat32(cjvalue->valuedouble),dboffset)});
                                sys_cfg.setDbVarIx(AnF32, dboffset, cjvalue->valuedouble);

                            }
                        }
                    }
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
                                commands.Add<ControlRelayOutputBlock>({WithIndex(ControlRelayOutputBlock(StringToControlCode(cjvalue->valuestring)),dboffset)});
                                // if(cjvalue->valueint == 1)
                                // {
                                //     commands.Add<ControlRelayOutputBlock>({WithIndex(ControlRelayOutputBlock(ControlCode::LATCH_ON),dboffset)});
                                // }
                                // else
                                // {
                                //     commands.Add<ControlRelayOutputBlock>({WithIndex(ControlRelayOutputBlock(ControlCode::LATCH_OFF),dboffset)});
                                // }
                            }
                        }
                    }
                    printf(" *****Running Direct Outputs \n");
                    master->DirectOperate(std::move(commands), PrintingCommandCallback::Get());
                }
            }
            //            dboffset = sys_cfg.getAnalogIdx(offset->valuestring);
            if (body_JSON != NULL)
            {
                cJSON_Delete(body_JSON);
            }
            p_fims->free_message(msg);
            // TODO delete fims message
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
            fprintf(stderr, "Failed to get fims socket.\n");
            rc = 1;
            goto cleanup;
        }

        fd_max = (fd_max > fims_socket) ? fd_max: fims_socket;
        fprintf(stderr, "Fims Setup complete: now for DNP3\n");

        while(running)
        {
            connections_with_data = all_connections;
            // Select will block until one of the file descriptors has data
            if(-1 == select(fd_max+1, &connections_with_data, NULL, NULL, NULL))
            {
                fprintf(stderr, "server select() failure: %s.\n", strerror(errno));
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
                            fprintf(stderr, "Fims connection closed.\n");
                            FD_CLR(current_fd, &all_connections);
                            break;
                        }
                        else
                            fprintf(stderr, "No fims message. Select led us to a bad place.\n");
                    }
                    else
                    {
                        process_fims_message(msg, server_map);
                        server_map->p_fims->free_message(msg);
                    }
                }
            }
        }

        fprintf(stderr, "Main loop complete: Entering clean up.\n");
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


