/*
 * dnp3_utils.cpp
 *
 *  Created on: Oct 2, 2018 - modbus
 *              May 4, 2020 - dp3
 *      Author: jcalcagni - modbus
  *             pwilshire - dnp3
 */
#include <stdio.h>
#include <cjson/cJSON.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <signal.h>
#include <arpa/inet.h>
#include <fims/libfims.h>

#include "dnp3_utils.h"
#include <map>
#include <string>
#include <cstring>
#include "opendnp3/app/AnalogOutput.h"
#include "opendnp3/app/ControlRelayOutputBlock.h"
#include "opendnp3/gen/CommandStatus.h"
#include "opendnp3/gen/ControlCode.h"

using namespace opendnp3;

using namespace std;

// modbus code 
int establish_connection(system_config* config)
{
    struct ifreq ifr;
    struct sockaddr_in addr;
    int sock, yes;
    // modbus listen only accepts on INADDR_ANY so need to make own socket and hand it over
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1)
    {
        fprintf(stderr, "Failed to create socket: %s.\n", strerror(errno));
        return -1;
    }

    yes = 1;
    if (-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof(yes)))
    {
        fprintf(stderr, "Failed to set socket option reuse address: %s.\n", strerror(errno));
        close(sock);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    //read port out of config
    addr.sin_port = htons(config->port);

    if(config->eth_dev != NULL)
    {
        ifr.ifr_addr.sa_family = AF_INET;
        //read interface out of config file
        strncpy(ifr.ifr_name, config->eth_dev, IFNAMSIZ - 1);
        if(-1 == ioctl(sock, SIOCGIFADDR, &ifr))
        {
            fprintf(stderr, "Failed to get ip address for interface %s: %s.\n", ifr.ifr_name, strerror(errno));
            close(sock);
            return -1;
        }
        addr.sin_addr.s_addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
    }
    else if(config->ip_address != NULL)
        addr.sin_addr.s_addr = inet_addr(config->ip_address);
    else
        addr.sin_addr.s_addr = INADDR_ANY;

    if (-1 == bind(sock, (struct sockaddr *)&addr, sizeof(addr)))
    {
        fprintf(stderr, "Failed to bind socket: %s.\nIf trying to use a port less than 1024 run programs as sudo.\n", strerror(errno));
        close(sock);
        return -1;
    }

    if (-1 == listen(sock, 1))
    {
        fprintf(stderr, "Failed to listen on socket: %s.\n", strerror(errno));
        close(sock);
        return -1;
    }

    // Values passed to modbus_new_tcp are ignored since we are listening on our own socket
    //config->mb = modbus_new_tcp("127.0.0.1", MODBUS_TCP_DEFAULT_PORT);
    //if(config->mb == NULL)
    //{
    //    fprintf(stderr, "Failed to allocate context.\n");
    //    close(sock);
    //    return -1;
    //}
    //if(config->device_id != -1 && -1 == modbus_set_slave(config->mb, config->device_id))
    //{
    //    fprintf(stderr, "Valid Unit identifiers (Device_Id) are between 1-247.\n");
    //    modbus_free(config->mb);
    //    config->mb = NULL;
    //    return -1;
    //}
    return sock;
}

void emit_event(fims* pFims, const char* source, const char* message, int severity)
{
    cJSON* body_object;
    body_object = cJSON_CreateObject();
    cJSON_AddStringToObject(body_object, "source", source);
    cJSON_AddStringToObject(body_object, "message", message);
    cJSON_AddNumberToObject(body_object, "severity", severity);
    char* body = cJSON_PrintUnformatted(body_object);
    pFims->Send("post", "/events", NULL, body);
    free(body);
    cJSON_Delete(body_object);
}

DbVar* getDbVar(sysCfg *cfgdb, const char *name)
{
    return cfgdb->getDbVar(name);
}

//old version deprecated
void pubWithTimeStamp(cJSON *cj, sysCfg* sys, const char* ev)
{
    if(cj)
    {
        addCjTimestamp(cj, "Timestampxx");
        char *out = cJSON_PrintUnformatted(cj);
        //cJSON_Delete(cj);
        //cj = NULL;
        if (out) 
        {
            char tmp[1024];
            if(ev) 
            {
                snprintf(tmp,1024,"/%s/%s/%s/%s", "id", sys->pub, ev, sys->id);
            }
            else
            {
                snprintf(tmp,1024,"/%s/%s/%s", "id", sys->pub, sys->id);
            }
            if(sys->p_fims)
            {
               sys->p_fims->Send("pub", tmp, NULL, out);
            }
            else
            {
                std::cout << __FUNCTION__ << " Error in sys->p_fims\n";
            }
            free(out);
        }
    }
}

//uses the ev field
void pubWithTimeStamp2(cJSON *cj, sysCfg* sys, const char* ev)
{
    if(cj)
    {    
        addCjTimestamp(cj, "Timestamp");
        char *out = cJSON_PrintUnformatted(cj);
        //cJSON_Delete(cj);
        //    cj = NULL;
        if (out) 
        {
            char tmp[1024];
            if(ev == NULL) 
            {
                snprintf(tmp,1024,"/%s/%s", "/components", sys->id);

            }
            else
            {
                snprintf(tmp,1024,"/%s/%s", ev, sys->id);
            }

            if(sys->p_fims)
            {
               sys->p_fims->Send("pub", tmp, NULL, out);
            }
            else
            {
                std::cout << __FUNCTION__ << " Error in sys->p_fims\n";
            }
        
            free(out);
        }
    }
}

void addCjTimestamp(cJSON *cj, const char* ts)
{
    char buffer[64],time_str[256];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm local_tm;
    tzset();
    strftime(buffer,sizeof(buffer),"%m-%d-%Y %T.", localtime_r(static_cast<time_t*>(&tv.tv_sec), &local_tm));
    snprintf(time_str, sizeof (time_str),"%s%ld",buffer,tv.tv_usec);
    cJSON_AddStringToObject(cj, ts, time_str);
}

cJSON* get_config_json(int argc, char* argv[])
{
    FILE *fp = NULL;

    if(argc <= 1)
    {
        printf("Need to pass argument of name.\n");
        return NULL;
    }

    if (argv[1] == NULL)
    {
        printf(" Failed to get the path of the test file. \n");
        return NULL;
    }
    //Open file
    fp = fopen(argv[1], "r");
    if(fp == NULL)
    {
        printf( "Failed to open file %s\n", argv[1]);
        return NULL;
    }

    // obtain file size
    fseek(fp, 0, SEEK_END);
    long unsigned file_size = ftell(fp);
    rewind(fp);

    // create Configuration_file and read file in Configuration_file
    char *config_file = new char[file_size];
    if(config_file == NULL)
    {
        fprintf(stderr, "Memory allocation error\n");
        fclose(fp);
        return NULL;
    }

    size_t bytes_read = fread(config_file, 1, file_size, fp);
    fclose(fp);
    if(bytes_read != file_size)
    {
        fprintf(stderr, "Read error.\n");
        delete[] config_file;
        return NULL;
    }

    cJSON* config = cJSON_Parse(config_file);
    delete[] config_file;
    if(config == NULL)
        fprintf(stderr, "Invalid JSON object in file\n");
    return config;
}

// new parser for sample2
cJSON *parseJSONConfig(char *file_path)
{
    //Open a file//
    if (file_path == NULL)
    {
        FPS_ERROR_PRINT(" Failed to get the path of the test file. \n"); //a check to make sure that args[1] is not NULL//
        return NULL;
    }
    FILE* fp = fopen(file_path, "r");
    if (fp == NULL)
    {
        FPS_ERROR_PRINT("Failed to open file %s\n", file_path);
        return NULL;
    }
    else FPS_DEBUG_PRINT("Opened file %s\n", file_path);
    // obtain file size

    fseek(fp, 0, SEEK_END);
    long unsigned file_size = ftell(fp);
    rewind(fp);
    // create configuration file

    char* configFile = new char[file_size];
    if (configFile == NULL)
    {
        FPS_ERROR_PRINT("Memory allocation error config file\n");
        fclose(fp);
        return NULL;
    }
    size_t bytes_read = fread(configFile, 1, file_size, fp);
    if (bytes_read != file_size)
    {
        FPS_ERROR_PRINT("Read error: read: %lu, file size: %lu .\n", (unsigned long)bytes_read, (unsigned long)file_size);
        fclose(fp);
        delete[] configFile;
        return NULL;
    }
    else
       FPS_DEBUG_PRINT("File size %lu\n", file_size);

    fclose(fp);
    cJSON* pJsonRoot = cJSON_Parse(configFile);
    delete[] configFile;
    if (pJsonRoot == NULL)
        FPS_ERROR_PRINT("cJSON_Parse returned NULL.\n");
    return(pJsonRoot);
}

// "system": {
//        "protocol": "DNP3",
//        "version": 1,
//        "id": "dnp3_outstation",
//        "ip_address": "192.168.1.50",
//        "port": 502,
//        "local_address": 1,
//		"remote_address": 10
//    },const char *iotypToStr (int t)

ControlCode TypeToControlCode(uint8_t arg)
{
  return static_cast<ControlCode>(arg);
}


int addVarToCj(cJSON* cj, DbVar* db, int flag)
{
    int rc = 0;
    const char* dname = db->name;
    switch (db->type)
    {

        case Type_Analog:
        {
            if(flag)
            {
                cJSON* cji = cJSON_CreateObject();
                cJSON_AddNumberToObject(cji, "value", db->valuedouble);
                cJSON_AddItemToObject(cj, dname, cji);
            }
            else
            {
                cJSON_AddNumberToObject(cj, dname, db->valuedouble);
            }
            break;
        }
        case Type_Binary:
        {
            if(flag)
            {
                cJSON* cji = cJSON_CreateObject();
                cJSON_AddBoolToObject(cji, "value", db->valueint);
                cJSON_AddItemToObject(cj, dname, cji);
            }
            else
            {
                cJSON_AddBoolToObject(cj, dname, db->valueint);
            }
            break;
        }
        case Type_Crob:
        {
            FPS_DEBUG_PRINT("*** %s Found variable [%s] type  %d crob %u [%s] \n"
                    , __FUNCTION__, dname, db->type, db->crob,ControlCodeToString(TypeToControlCode(db->crob)));
            if(flag)
            {
                cJSON* cji = cJSON_CreateObject();
                cJSON_AddStringToObject(cji, "value", ControlCodeToString(TypeToControlCode(db->crob)));
                cJSON_AddItemToObject(cj, dname, cji);
            }
            else
            {
                cJSON_AddStringToObject(cj, dname, ControlCodeToString(TypeToControlCode(db->crob)));
            }
            break;
        }
        case AnIn16:
        {
            if(flag)
            {
                cJSON* cji = cJSON_CreateObject();
                cJSON_AddNumberToObject(cji, "value", db->anInt16);
                cJSON_AddItemToObject(cj, dname, cji);
            }
            else
            {
                cJSON_AddNumberToObject(cj, dname, db->anInt16);
            }
            break;
        }
        case AnIn32:
        {
            if(flag)
            {
                cJSON* cji = cJSON_CreateObject();
                cJSON_AddNumberToObject(cji, "value", db->anInt32);
                cJSON_AddItemToObject(cj, dname, cji);
            }
            else
            {
                cJSON_AddNumberToObject(cj, dname, db->anInt32);
            }
            break;
        }
        case AnF32:
        {
            if(flag)
            {
                cJSON* cji = cJSON_CreateObject();
                cJSON_AddNumberToObject(cji, "value", db->anF32);
                cJSON_AddItemToObject(cj, dname, cji);
            }
            else
            {
                cJSON_AddNumberToObject(cj, dname, db->anF32);
            }
            break;
        }
        default:
        {
            rc = -1;
            FPS_ERROR_PRINT("%s  unknown db_->type : %d\n"
                            , __FUNCTION__
                            , db->type
                            );
            break;
        }
    }
    return rc;
}

int addVarToCj(cJSON* cj,std::pair<DbVar*,int>dbp)
{
    
    DbVar* db = dbp.first;
    int flag = dbp.second;
    return addVarToCj(cj, db, flag);
}

int addVarToCj(cJSON* cj, DbVar* db)
{
    return addVarToCj(cj, db, 0);
}

int addVarToCj(sysCfg* sys, cJSON* cj, const char* dname)
{
    DbVar* db = sys->getDbVar(dname);

    return addVarToCj(cj, db, 0);
}

const char* dreg_types[] = { "AnOPInt16", "AnOPInt32", "AnOPF32", "CROB", "analog", "binary",0 };

const char *iotypToStr (int t)
{
    if (t < Type_of_Var::NumTypes)
    {
        return dreg_types[t];
    }
    return "Unknown";
}

int iotypToId (const char* t)
{
    int i;
    for (i = 0; i < Type_of_Var::NumTypes; i++)
    {
        if (strcmp(t , dreg_types[i] ) == 0 )
        return i;
    }
    return -1;
}

bool getCJint (cJSON *cj, const char *name, int& val, bool required)
{
    bool ok = !required;
    cJSON *cji = cJSON_GetObjectItem(cj, name);
    if (cji) {
        val = cji->valueint;
        ok = true;
    }
    return ok;
}

bool getCJstr (cJSON *cj, const char *name, char *& val, bool required)
{
    bool ok = !required;
    cJSON *cji = cJSON_GetObjectItem(cj, name);
    if (cji) {
        if(val) free(val);
        val = strdup(cji->valuestring);
        ok = true;
    }
    return ok;
}

bool parse_system(cJSON* cji, sysCfg* sys)
{
    bool ret = true;
    cJSON *cj = cJSON_GetObjectItem(cji, "system");

    if (cj == NULL)
    {
        FPS_ERROR_PRINT("system  missing from file! \n");
        ret = false;
    }
    
    if(ret) ret = getCJint(cj,"version",         sys->version       ,true );
    if(ret) ret = getCJint(cj,"port",            sys->port          ,true);
    if(ret) ret = getCJint(cj,"local_address",   sys->local_address ,true);
    if(ret) ret = getCJint(cj,"remote_address",  sys->remote_address,true);
    if(ret) ret = getCJstr(cj,"id",              sys->id            ,true);
    if(ret) ret = getCJstr(cj,"protocol",        sys->protocol      ,true);
    if(ret) ret = getCJstr(cj,"ip_address",      sys->ip_address    ,true);
    if(ret) ret = getCJstr(cj,"pub",             sys->pub           ,false);
    // config file has "objects" with children groups "binary" and "analog"
    return ret;
}

int  parse_object(sysCfg* sys, cJSON* objs, int idx)
{

    cJSON *id, *offset, *uri, *bf, *bits;

    cJSON *JSON_list = cJSON_GetObjectItem(objs, iotypToStr (idx));
    if (JSON_list == NULL)
    {
        FPS_ERROR_PRINT("[%s] objects missing from config! \n",iotypToStr (idx));
        return -1;
    }

    uint num = cJSON_GetArraySize(JSON_list);
    sys->numObjs[idx] = 0; 
    for(uint i = 0; i < num; i++)
    {
        cJSON* obj = cJSON_GetArrayItem(JSON_list, i);
        if(obj == NULL)
        {
            FPS_ERROR_PRINT("Invalid or NULL binary at %d\n", i);
            continue;
        }
        id = cJSON_GetObjectItem(obj, "id");
        offset = cJSON_GetObjectItem(obj, "offset");
        uri = cJSON_GetObjectItem(obj, "uri");
        bf = cJSON_GetObjectItem(obj, "bit_field");
        bits = cJSON_GetObjectItem(obj, "bit_strings");
        //"bit_field": true,
        //"bit_strings": [
            // "some String"

        if (id == NULL || offset == NULL || id->valuestring == NULL)
        {
            FPS_ERROR_PRINT("NULL variables or component_id for %d\n", i);
            continue;
        }
        DbVar* db = sys->addDbVar(id->valuestring, idx, offset->valueint, uri?uri->valuestring:NULL);
        if (bf && bits && (bits->type == cJSON_Array))
        {

            //TODO bits need a value use make_pair there too 
            FPS_ERROR_PRINT("*****************Adding bitfields for %s\n", db->name;
            sys->addBits(db, bits);

        }
        FPS_DEBUG_PRINT(" config adding name [%s] id [%d]\n", id->valuestring, offset->valueint);
        sys->numObjs[idx]++; 
        if (uri) 
        {
            sys->addUri(uri, db);
        }
        else
        {
            sys->addDefUri(db);
        }
        
    }
    return  sys->numObjs[idx]; 
}

bool parse_variables(cJSON* object, sysCfg* sys)
{
    // config file has "objects" with children groups "binary" and "analog"
    cJSON *JSON_objects = cJSON_GetObjectItem(object, "objects");
    if (JSON_objects == NULL)
    {
        FPS_ERROR_PRINT("objects object is missing from file! \n");
        return false;
    }
    for (int idx = 0; idx< Type_of_Var::NumTypes; idx++)
        parse_object(sys, JSON_objects, idx);
    return true;
}

int parse_system(cJSON *system, system_config *config)
{
    memset(config, 0, sizeof(system_config));
    cJSON *system_name = cJSON_GetObjectItem(system, "id");
    if (system_name == NULL || system_name->valuestring == NULL)
    {
        fprintf(stderr, "Failed to find system 'id' in JSON.\n");
        return -1;
    }
    config->name = strdup(system_name->valuestring);
    if(config->name == NULL)
    {
        fprintf(stderr, "Allocation failed for name");
        return -1;
    }

    cJSON* freq = cJSON_GetObjectItem(system, "frequency");
    config->frequency = (freq != NULL && freq->type == cJSON_Number) ? freq->valueint : 500;

    cJSON *off_by_one = cJSON_GetObjectItem(system, "off_by_one");
    config->off_by_one = (off_by_one != NULL && off_by_one->type == cJSON_True);

    cJSON *byte_swap = cJSON_GetObjectItem(system, "byte_swap");
    config->byte_swap = (byte_swap != NULL && byte_swap->type == cJSON_True);

    //TODO add support for eth device instead of ip

    // get connection information from object system
    cJSON* ip_addr = cJSON_GetObjectItem(system, "ip_address");
    cJSON* serial_device = cJSON_GetObjectItem(system, "serial_device");
    if (ip_addr != NULL)
    {
        cJSON* port = cJSON_GetObjectItem(system, "port");
        config->port = (port == NULL) ? 502 : port->valueint;
        if(config->port > 65535 || config->port < 0)
        {
            fprintf(stderr, "Invalid port defined.\n");
            free(config->name);
            return -1;
        }
        if(ip_addr->valuestring == NULL)
        {
            fprintf(stderr, "Invalid ip address string.\n");
            free(config->name);
            return -1;
        }
        config->ip_address = strdup(ip_addr->valuestring);
        if(config->ip_address == NULL)
        {
            fprintf(stderr, "Allocations error ip address string.\n");
            free(config->name);
            return -1;
        }
    }
    else if (serial_device != NULL)
    {
        cJSON* baud_rate = cJSON_GetObjectItem(system, "baud_rate");
        config->baud = (baud_rate == NULL) ? 115200 : baud_rate->valueint;
        cJSON* parity_str = cJSON_GetObjectItem(system, "parity");
        config->parity = 'N';
        // if parity field exists make sure it is valid
        if(parity_str != NULL)
        {
            if(parity_str->valuestring == NULL)
            {
                fprintf(stderr, "Invalid Parity value in json\n");
                free(config->name);
                return -1;
            }
            else if(strcmp("none", parity_str->valuestring) == 0)
                config->parity = 'N';
            else if(strcmp("even", parity_str->valuestring) == 0)
                config->parity = 'E';
            else if(strcmp("odd", parity_str->valuestring) == 0)
                config->parity = 'O';
            else
            {
                fprintf(stderr, "Invalid Parity value in json\n");
                free(config->name);
                return -1;
            }
        }

        cJSON* data_bits = cJSON_GetObjectItem(system, "data_bits");
        config->data = (data_bits == NULL) ? 8 : data_bits->valueint;
        if(config->data > 8 || config->data < 5)
        {
            fprintf(stderr, "Invalid number of data bits.\n");
            free(config->name);
            return -1;
        }

        cJSON* stop_bits = cJSON_GetObjectItem(system, "stop_bits");
        config->stop = (stop_bits == NULL) ? 1 : stop_bits->valueint;
        if(config->stop > 2 || config->stop < 1)
        {
            fprintf(stderr, "Invalid number of stop bits.\n");
            free(config->name);
            return -1;
        }

        if(serial_device->valuestring == NULL)
        {
            fprintf(stderr, "Invalid string for serial device.\n");
            free(config->name);
            return -1;
        }
        config->serial_device = strdup(serial_device->valuestring);
        if(config->serial_device == NULL)
        {
            fprintf(stderr, "Allocation error string for serial device.\n");
            free(config->name);
            return -1;
        }
    }
    else
    {
        fprintf(stderr, "Failed to find either an IP_Address or Serial_Device in JSON.\n");
        free(config->name);
        return -1;
    }

    // Device ID is required for serial
    cJSON* slave_id = cJSON_GetObjectItem(system, "device_id");
    config->device_id = -1;
    // if device_id is given check that it is valid
    if(slave_id != NULL)
    {
        if(slave_id->valueint < 0 || slave_id->valueint > 255)
        {
            fprintf(stderr, "Invalid Device_ID.\n");
            free(config->name);
            if(config->ip_address != NULL)
                free(config->ip_address);
            if(config->serial_device != NULL)
                free(config->serial_device);
            return -1;
        }
        config->device_id = slave_id->valueint;
    }
    return 0;
}

//std::vector<std::pair<DbVar*,int>>dbs; // collect all the parsed vars here
cJSON* parseBody(dbs_type& dbs, sysCfg*sys, fims_message*msg, const char* who)
{
    const char* uri = NULL;
    int fragptr = 1;
    cJSON* body_JSON = cJSON_Parse(msg->body);
    cJSON* itypeA16 = NULL;
    cJSON* itypeA32 = NULL;
    cJSON* itypeF32 = NULL;
    cJSON* itypeValues = NULL;
    cJSON* itypeCROB = NULL;
    cJSON* cjit = NULL;
    
    if (body_JSON == NULL)
    {
        if(strcmp(msg->method,"set") == 0)
        {
            FPS_ERROR_PRINT("fims message body is NULL or incorrectly formatted for  method (%s) uri (%s) \n", msg->method, msg->uri);
            if(msg->body != NULL)
            {
                FPS_ERROR_PRINT("fims message body (%s) \n", msg->body);
            }
            return NULL;
        }
    }
    
    if (msg->nfrags < 2)
    {
        FPS_ERROR_PRINT("fims message not enough pfrags id [%s] \n", sys->id);
        return body_JSON;
    }

    uri = msg->pfrags[fragptr];
    if (strncmp(uri, who, strlen(who)) != 0)
    {
        FPS_ERROR_PRINT("fims message msg->uri [%s] frag 1 [%s] not for   [%s] \n", msg->uri, uri, who);
        return body_JSON;
    }
    else
    {
        FPS_ERROR_PRINT("fims message ACCEPTED msg->uri [%s] body [%s] \n", msg->uri, msg->body);
        fragptr = 1;
    }              
    uri = msg->pfrags[fragptr+1];

    // TODO look for the interfaces response

    FPS_DEBUG_PRINT(" %s Running with uri: [%s] \n", __FUNCTION__, uri);
    if (strncmp(uri, sys->id, strlen(sys->id)) != 0)
    {
        FPS_ERROR_PRINT("fims message frag %d [%s] not for this %s [%s] \n", fragptr+1, uri, who, sys->id);
        return body_JSON;
    }
    // set /components/master/dnp3_outstation '{"name1":1, "name2":23.56}'
    // or 
    // set /components/master/dnp3_outstation '{"name1":{"value":1}, "name2":{"value":2}'}
    // TODO add pubs for outstation
    if((strcmp(msg->method,"set") != 0) && (strcmp(msg->method,"get") != 0))
    {
        FPS_ERROR_PRINT("fims unsupported method [%s] \n", msg->method);
        return body_JSON;
    }
    // this is the "debug" or development method
    //-m set -u "/components/<some_master_id>/[<some_outstation_id>] '{"AnalogInt32": [{"offset":"name_or_index","value":52},{"offset":2,"value":5}]}' 

    // get is OK codewise..
    if(strcmp(msg->method, "get") == 0)
    {
        FPS_ERROR_PRINT("fims method [%s] almost  supported for [%s]\n", msg->method, who);

        // is this a singleton ? 
        if ((int)msg->nfrags > fragptr+2)
        {
            int flag = 0;
            uri = msg->pfrags[fragptr+2];  // TODO check for delim. //components/master/dnp3_outstation/line_voltage/stuff
            FPS_DEBUG_PRINT("fims message frag %d variable name [%s] \n", fragptr+2,  uri);
            DbVar* db = sys->getDbVar(uri);
            if (db != NULL)
            {
                FPS_DEBUG_PRINT("Found variable [%s] type  %d \n", db->name, db->type); 
                dbs.push_back(std::make_pair(db, flag));
                return body_JSON;
            }
        }
        // else get them all
        else
        {
            sys->addVarsToVec(dbs);
            return body_JSON;
        }
        return body_JSON;
    }

    if(strcmp(msg->method,"set") == 0)
    {
        int single = 0;
        //int reply = 1;
        // watch out for sets on /interfaces/outstation/dnp3_outstation/reply/dnp3_outstation
        // handle a single item set  crappy code for now, we'll get a better plan in a day or so 
        FPS_ERROR_PRINT("fims method [%s] uri [%s] almost  supported for [%s]\n", msg->method, msg->uri, who);

        if(strstr(msg->uri, "/reply/") != NULL)
        {
            FPS_DEBUG_PRINT("fims message reply uri DETECTED  [%s] \n", msg->uri);
            //single = 0;
        }

        if ((int)msg->nfrags > fragptr+2)
        {
            uri = msg->pfrags[fragptr+2];  // TODO check for delim. //components/master/dnp3_outstation/line_voltage/stuff
            if(strstr(msg->uri, "/reply/") != NULL)
            {
                FPS_DEBUG_PRINT("fims message reply uri ACCEPTED  [%s] \n", msg->body);
                single = 0;
            }
            else
            {
            
                FPS_DEBUG_PRINT("fims message frag %d SINGLE variable name [%s] \n", fragptr+2,  uri);
                single = 1;
            }
        }

        if(single == 1)
        {
            // process a single var
            DbVar* db = sys->getDbVar(uri);
            if (db != NULL)
            {
                int flag = 0;
                FPS_DEBUG_PRINT("Found variable type  %d \n", db->type);
                itypeValues = body_JSON;
                // allow '"string"' OR '{"value":"string"}'
                if(itypeValues->type == cJSON_Object)
                {
                    flag = 1;
                    itypeValues = cJSON_GetObjectItem(itypeValues, "value");
                }
                // Only Crob gets a string 
                if(itypeValues && (itypeValues->type == cJSON_String))
                {
                    if(db->type == Type_Crob)
                    {
                        uint8_t cval = ControlCodeToType(StringToControlCode(itypeValues->valuestring));
                        sys->setDbVarIx(Type_Crob, db->offset, cval);
                        // send the response
                        dbs.push_back(std::make_pair(db, flag));

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
            return body_JSON;
        }

        // scan the development options  -- these also get replies 
        itypeA16 = cJSON_GetObjectItem(body_JSON, "AnOPInt16");
        itypeA32 = cJSON_GetObjectItem(body_JSON, "AnOPInt32");
        itypeF32 = cJSON_GetObjectItem(body_JSON, "AnOPF32");
        itypeCROB = cJSON_GetObjectItem(body_JSON, "CROB");
        //the value list does get a reply
        itypeValues = body_JSON;

        // process {"valuex":xxx,"valuey":yyy} ; xxx or yyy could be a number or {"value":val}
        if ((itypeA16 == NULL) && (itypeA32 == NULL) && (itypeF32 == NULL) && (itypeCROB == NULL)) 
        {
            FPS_DEBUG_PRINT("Found variable list or array \n");

            // decode values may be in an array 
            if (cJSON_IsArray(itypeValues)) 
            {
                FPS_DEBUG_PRINT("Found variable array \n");

                cJSON_ArrayForEach(cjit, itypeValues) 
                {
                    cJSON* cjo = cJSON_GetObjectItem(cjit, "offset");
                    cJSON* cjv = cJSON_GetObjectItem(cjit, "value");
                    addValueToVec(dbs, sys, cjo->valuestring, cjv, 0);
                }
            }
            else
            {
                // process a simple list
                cjit = itypeValues->child;
                FPS_DEBUG_PRINT("****** Start with variable list iterator->type %d\n\n", cjit->type);

                while(cjit != NULL)
                {   
                    FPS_DEBUG_PRINT("Found variable name  [%s] child %p \n"
                                            , cjit->string
                                            , (void *)cjit->child
                                            );
                    addValueToVec(dbs, sys, cjit->string, cjit, 0);
                    cjit = cjit->next;
                }
                FPS_DEBUG_PRINT("***** Done with variable list \n\n");
            }
            return body_JSON;//return dbs.size();
        }
    }
    return body_JSON;//return dbs.size();
}

// need nodbs option
int addValueToVec(dbs_type& dbs, sysCfg*sys, const char* name , cJSON *cjvalue, int flag)
{
    // cjoffset must be a name
    // cjvalue may be an object

    if (name == NULL)
    {
        FPS_ERROR_PRINT(" ************** %s offset is not  string\n",__FUNCTION__);
        return -1;
    }

    DbVar* db = getDbVar(sys, name); 
    if (db == NULL)
    {
        FPS_ERROR_PRINT( " ************* %s Var [%s] not found in dbMap\n", __FUNCTION__, name);
        return -1;
    }
    
    FPS_DEBUG_PRINT(" ************* %s Var [%s] found in dbMap\n", __FUNCTION__, name);

    if (cjvalue->type == cJSON_Object)
    {
        flag = 1;
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
        sys->setDbVar(name, cjvalue);
        dbs.push_back(std::make_pair(db, flag));
    }
    else if (db->type == AnIn32) 
    {
        //commands.Add<AnalogOutputInt32>({WithIndex(AnalogOutputInt32(cjvalue->valueint),pt->offset)});
        sys->setDbVar(name, cjvalue);
        dbs.push_back(std::make_pair(db, flag));
    }
    else if (db->type == AnF32) 
    {
        //commands.Add<AnalogOutputFloat32>({WithIndex(AnalogOutputFloat32(cjvalue->valuedouble),pt->offset)});
        sys->setDbVar(name, cjvalue);
        dbs.push_back(std::make_pair(db, flag));
    }
    else if (db->type == Type_Crob) 
    {
        FPS_DEBUG_PRINT(" ************* %s Var [%s] CROB setting value [%s]  to %d \n"
                                                    , __FUNCTION__
                                                    , db->name
                                                    , name
                                                    , (int)StringToControlCode(cjvalue->valuestring)
                                                    );

        sys->setDbVar(name, cjvalue);
        dbs.push_back(std::make_pair(db, flag));
    }
    else if (db->type == Type_Analog) 
    {
        //commands.Add<AnalogOutputInt16>({WithIndex(AnalogOutputInt16(cjvalue->valueint), db->offset)});
        sys->setDbVar(name, cjvalue);
        dbs.push_back(std::make_pair(db, flag));
    }
    else if (db->type == Type_Binary) 
    {
        //commands.Add<AnalogOutputInt16>({WithIndex(AnalogOutputInt16(cjvalue->valueint), db->offset)});
        sys->setDbVar(name, cjvalue);
        dbs.push_back(std::make_pair(db, flag));
    }

    else
    {
      FPS_ERROR_PRINT( " *************** %s Var [%s] not processed \n",__FUNCTION__, name);  
      return -1;
    }
    return dbs.size();   
}

// need nodbs option  or do we ??
int addValueToDb(sysCfg*sys, const char* name , cJSON *cjvalue, int flag)
{
    // cjoffset must be a name
    // cjvalue may be an object

    if (name == NULL)
    {
        FPS_ERROR_PRINT(" ************** %s offset is not  string\n",__FUNCTION__);
        return -1;
    }

    DbVar* db = getDbVar(sys, name); 
    if (db == NULL)
    {
        FPS_ERROR_PRINT( " ************* %s Var [%s] not found in dbMap\n", __FUNCTION__, name);
        return -1;
    }
    
    FPS_DEBUG_PRINT(" ************* %s Var [%s] found in dbMap\n", __FUNCTION__, name);

    if (cjvalue->type == cJSON_Object)
    {
        flag = 1;
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
        sys->setDbVar(name, cjvalue);
        //dbs.push_back(std::make_pair(db, flag));
    }
    else if (db->type == AnIn32) 
    {
        //commands.Add<AnalogOutputInt32>({WithIndex(AnalogOutputInt32(cjvalue->valueint),pt->offset)});
        sys->setDbVar(name, cjvalue);
        //dbs.push_back(std::make_pair(db, flag));
    }
    else if (db->type == AnF32) 
    {
        //commands.Add<AnalogOutputFloat32>({WithIndex(AnalogOutputFloat32(cjvalue->valuedouble),pt->offset)});
        sys->setDbVar(name, cjvalue);
        //dbs.push_back(std::make_pair(db, flag));
    }
    else if (db->type == Type_Crob) 
    {
        FPS_DEBUG_PRINT(" ************* %s Var [%s] CROB setting value [%s]  to %d \n"
                                                    , __FUNCTION__
                                                    , db->name
                                                    , name
                                                    , (int)StringToControlCode(cjvalue->valuestring)
                                                    );

        sys->setDbVar(name, cjvalue);
        //dbs.push_back(std::make_pair(db, flag));
    }
    else
    {
      FPS_ERROR_PRINT( " *************** %s Var [%s] not found in dbMap\n",__FUNCTION__, name);  
      return -1;
    }
    return 0;//dbs.size();   
}

server_data* create_register_map(cJSON* registers, datalog* data)
{
    int i, j;
    unsigned int k;
    uri_map::iterator it;
    body_map::iterator body_it;
    const char* reg_types[] = { "coils", "discrete_inputs", "input_registers", "holding_registers", 0 };
    memset(data, 0, sizeof(datalog)*Num_Register_Types);
    server_data* srv_data = new server_data;
    if(srv_data == NULL)
    {
        fprintf(stderr,"Allocation error.\n");
        return NULL;
    }
    for(i = 0; i < Num_Register_Types; i++)
    { 
        fprintf(stdout, "Seeking registers included in [%s] config object.\n", reg_types[i]);
        cJSON* reg_set = cJSON_GetObjectItem(registers, reg_types[i]);
        if(reg_set != NULL)
        {
            fprintf(stdout, ">>>>Found registers included in [%s] config object.\n", reg_types[i]);

            data[i].reg_type = (Type_of_Register)i;
            int reg_cnt = data[i].map_size = cJSON_GetArraySize(reg_set);
            if(reg_cnt == 0)
            {
                fprintf(stderr, "No registers included in %s config object.\n", reg_types[i]);
                continue;
            }
            data[i].register_map = new maps[reg_cnt];
            int min_offset, max_offset;
            min_offset = max_offset = -1;
            for(j = 0; j < reg_cnt; j++)
            {
                int offset, num_regs;
                cJSON* cur_reg = cJSON_GetArrayItem(reg_set, j);
                if(cur_reg == NULL ||cur_reg->type != cJSON_Object)
                {
                    fprintf(stderr, "Error: Failed to read or in invalid format: %s register entry %d.\n", reg_types[i], i);
                    goto cleanup;
                }
                char* spreg = cJSON_Print(cur_reg);
                fprintf(stderr, "Working with %s array item %d.\n", spreg, j);

                cJSON* cur_off = cJSON_GetObjectItem(cur_reg, "offset");
                cJSON* cur_uri = cJSON_GetObjectItem(cur_reg, "uri");
                cJSON* cur_id  = cJSON_GetObjectItem(cur_reg, "id");
                // hack for now
                if(cur_uri == NULL) cur_uri = cur_id;

                if(cur_off == NULL || cur_off->type != cJSON_Number ||
                   cur_uri == NULL || cur_uri->type != cJSON_String ||
                   cur_id  == NULL || cur_id->type  != cJSON_String  )
                {
                    fprintf(stderr, "Error: Missing required field for %s register j %d entry %d.\n", reg_types[i], j, i);
                    goto cleanup;
                }

                data[i].register_map[j].uri      = strdup(cur_uri->valuestring);
                data[i].register_map[j].reg_name = strdup(cur_id->valuestring);
                data[i].register_map[j].reg_off  = offset = cur_off->valueint;

                it = srv_data->uri_to_register.find(data[i].register_map[j].uri);
                if(it == srv_data->uri_to_register.end())
                {
                    // first time uri is used insert new map
                    std::pair<uri_map::iterator, bool> rtn_pair = srv_data->uri_to_register.insert(std::pair<const char*, body_map*>(data[i].register_map[j].uri, new body_map));
                    it = rtn_pair.first;
                    if(rtn_pair.second == false || it->second == NULL)
                    {
                        fprintf(stderr, "Failed to insert map entry for new uri.\n");
                        goto cleanup;
                    }
                }
                body_it = it->second->find(data[i].register_map[j].reg_name);
                if(body_it == it->second->end())
                {
                    maps** reg_info = new maps*[Num_Register_Types];
                    bool* reg_type = new bool[Num_Register_Types];
                    memset(reg_info, 0, sizeof(maps*) * Num_Register_Types);
                    memset(reg_type, 0, sizeof(bool) * Num_Register_Types);
                    reg_info[i] = &(data[i].register_map[j]);
                    reg_type[i] = true;
                    std::pair<body_map::iterator, bool> rtn_pair = it->second->insert(std::pair<const char*, std::pair<bool*, maps**> >(data[i].register_map[j].reg_name, std::pair<bool*, maps**> (reg_type, reg_info)));
                    if(rtn_pair.second == false)
                    {
                        fprintf(stderr, "Failed to insert map entry for new %s: %s.\n", reg_types[i], data[i].register_map[j].reg_name);
                        goto cleanup;
                    }
                }
                else
                {
                    if(body_it->second.first[i] == true || body_it->second.second[i] != NULL)
                    {
                        //Already taken
                        fprintf(stderr, "Failed to insert map entry for new %s: %s. Base URI and Id already used in the map.\n", reg_types[i], data[i].register_map[j].reg_name);
                        goto cleanup;
                    }
                    body_it->second.first[i] = true;
                    body_it->second.second[i] = &(data[i].register_map[j]);
                }

                cJSON* multi_reg = cJSON_GetObjectItem(cur_reg,"size");
                data[i].register_map[j].num_regs = num_regs =
                    (multi_reg != NULL && multi_reg->type == cJSON_Number) ? multi_reg->valueint : 1;

                cJSON* bit_field = cJSON_GetObjectItem(cur_reg, "bit_field");
                data[i].register_map[j].bit_field = (bit_field != NULL && bit_field->type == cJSON_True);

                cJSON* is_signed = cJSON_GetObjectItem(cur_reg,"signed");
                data[i].register_map[j].sign = (is_signed != NULL && is_signed->type == cJSON_True);

                cJSON* is_bool = cJSON_GetObjectItem(cur_reg, "bool");
                data[i].register_map[j].is_bool = (is_bool != NULL && is_bool->type == cJSON_True);

                cJSON* scale_value = cJSON_GetObjectItem(cur_reg,"scale");
                data[i].register_map[j].scale = (scale_value != NULL) ? (scale_value->valuedouble) : 0.0;

                min_offset = (min_offset > offset || min_offset == -1) ? offset : min_offset;
                max_offset = (max_offset < (offset + num_regs)) ? (offset + num_regs) : max_offset;
            }
            // store size of register map
            data[i].start_offset = min_offset;
            data[i].num_regs = max_offset - min_offset;
            srv_data->regs_to_map[i] = new maps*[data[i].num_regs];
            memset(srv_data->regs_to_map[i], 0, sizeof(maps*) * data[i].num_regs);
            // building hash lookup for register
            // this has to be done in a second pass because
            // we don't know how big the register map will be until we
            // have read all the registers and got the min and max offsets
            for(j = 0; j < reg_cnt; j++)
            {
                data[i].register_map[j].reg_off -= min_offset;
                for(k = data[i].register_map[j].reg_off; k < data[i].register_map[j].reg_off + data[i].register_map[j].num_regs; k++)
                {
                    if(srv_data->regs_to_map[i][k] != NULL)
                    {
                        fprintf(stderr, "Error in json config: Collision, %s at offset %u is defined to multiple variables %s and %s.\n",
                                reg_types[i], k + min_offset, srv_data->regs_to_map[i][k]->reg_name, data[i].register_map[j].reg_name);
                        goto cleanup;
                    }
                    srv_data->regs_to_map[i][k] = data[i].register_map + j;
                }
            }
        }
    }
    //TODO this does not check if redundant URIs are added, IE a base and a child
    // copy URIs that are needed to subscribe to
    srv_data->num_uris = srv_data->uri_to_register.size();
    srv_data->uris = new const char*[srv_data->num_uris + 1];
    i = 1; // start with 1 because 0 is used for this processes uri
    for(it = srv_data->uri_to_register.begin(); it != srv_data->uri_to_register.end(); ++it, i++)
        srv_data->uris[i] = it->first;
#if 0
    srv_data->mb_mapping = modbus_mapping_new_start_address(data[Coil].start_offset, data[Coil].num_regs,
                                     data[Discrete_Input].start_offset,   data[Discrete_Input].num_regs,
                                     data[Holding_Register].start_offset, data[Holding_Register].num_regs,
                                     data[Input_Register].start_offset,   data[Input_Register].num_regs);

    if (srv_data->mb_mapping == NULL)
    {
        fprintf(stderr, "Failed to allocate the mapping: %s\n", modbus_strerror(errno));
        goto cleanup;
    }
#endif
    
    return srv_data;

    cleanup:
    for (it = srv_data->uri_to_register.begin(); it != srv_data->uri_to_register.end(); ++it)
    {
        for (body_it = it->second->begin(); body_it != it->second->end(); ++body_it)
        {
            delete []body_it->second.first;
            delete []body_it->second.second;
        }
        it->second->clear();
        delete(it->second);
    }
    srv_data->uri_to_register.clear();
    for (i = 0; i < Num_Register_Types; i++)
    {
        if(data[i].register_map != NULL)     delete [] data[i].register_map;
        if(srv_data->regs_to_map[i] != NULL) delete [] srv_data->regs_to_map[i];
    }
    if (srv_data->uris != NULL) delete []srv_data->uris;
    delete srv_data;
    return NULL;
}

bool update_register_value(dnp3_mapping_t* map, bool* reg_type, maps** settings, cJSON* value)
{
    // Handle value objects
    cJSON* obj = cJSON_GetObjectItem(value, "value");
    if (obj == NULL)
        obj = value;

    for(int i = 0; i < Num_Register_Types; i++)
    {
        if(reg_type[i] == false)
            continue;
        if((i == Coil) || (i == Discrete_Input))
        {
            uint8_t* regs = (i == Coil) ? map->tab_bits : map->tab_input_bits;
            regs[settings[i]->reg_off] = (obj->type == cJSON_True);
        }
        else if((i == Input_Register) || (i == Holding_Register))
        {
            uint16_t* regs = (i == Holding_Register) ? map->tab_registers : map->tab_input_registers;
            if(settings[i]->num_regs == 1)
            {
                uint16_t val;
                if(settings[i]->is_bool)
                {
                    val = static_cast<uint16_t> (obj->type == cJSON_True);
                }
                else if(settings[i]->bit_field)
                {
                    int size = cJSON_GetArraySize(value);
                    if(size != 0)
                    {
                        //get last item in array
                        cJSON* item = cJSON_GetArrayItem(value, size-1);
                        if (item != NULL)
                        {
                            obj = cJSON_GetObjectItem(item, "value");
                            val = (obj == NULL) ? 0 : obj->valueint;
                        }
                        else
                            val = 0;
                    }
                    else
                        val = 0;
                }
                else if(settings[i]->scale != 0.0)
                {
                    if(settings[i]->sign == true)
                        val = static_cast<int32_t> (obj->valuedouble * settings[i]->scale);
                    else
                        val = static_cast<uint16_t> (obj->valuedouble * settings[i]->scale);
                }
                else
                {
                    if(settings[i]->sign == true)
                        val = static_cast<int16_t> (obj->valueint);
                    else
                        val = static_cast<uint16_t> (obj->valueint);
                }
                regs[settings[i]->reg_off] = val;
            }
            else if(settings[i]->num_regs == 2)
            {
                uint32_t val;
                if(settings[i]->bit_field)
                {
                    int size = cJSON_GetArraySize(value);
                    if(size != 0)
                    {
                        //get last item in array
                        cJSON* item = cJSON_GetArrayItem(value, size-1);
                        if (item != NULL)
                        {
                            obj = cJSON_GetObjectItem(item, "value");
                            val = (obj == NULL) ? 0 : obj->valueint;
                        }
                        else
                            val = 0;
                    }
                    else
                        val = 0;
                }
                else if(settings[i]->scale != 0.0)
                {
                    if(settings[i]->sign == true)
                        val = static_cast<int32_t> (obj->valuedouble * settings[i]->scale);
                    else
                        val = static_cast<uint32_t> (obj->valuedouble * settings[i]->scale);
                }
                else
                {
                    if(settings[i]->sign == true)
                        val = static_cast<int32_t> (obj->valueint);
                    else
                        val = static_cast<uint32_t> (obj->valueint);
                }
                //TODO need to do byte swap here if we want to support
                regs[settings[i]->reg_off]     = static_cast<uint16_t> (val >> 16);
                regs[settings[i]->reg_off + 1] = static_cast<uint16_t> (val & 0x0000ffff);
            }
        }
    }
    return true;
}

cJSON* cfgdbFindAddArray(sysCfg* cfgdb, const char* field)
{
    // look for cJSON Object called field
    cJSON* cjf = cJSON_GetObjectItem(cfgdb->cj, field);
    if(!cjf)
    {
        cJSON* cja =cJSON_CreateArray();
        cJSON_AddItemToObject(cfgdb->cj,field,cja);
        cjf = cJSON_GetObjectItem(cfgdb->cj, field);
    }
    return cjf;
}

// possibly used in outstation comand handler to publish changes
void cfgdbAddtoRecord(sysCfg* cfgdb,const char* field, const opendnp3::AnalogOutputInt16& cmd, uint16_t index)
{
    cJSON* cjf = cfgdbFindAddArray(cfgdb, field);
    cJSON* cji = cJSON_CreateObject();
    // todo resolve name
    cJSON_AddNumberToObject(cji,"offset",index);
    //const char* indexName = cfgindextoName(AOP16,index);
    //cJSON_AddStringToObject(cji,"offset",indexName);
    cJSON_AddNumberToObject(cji,"value", cmd.value);
    cJSON_AddItemToArray(cjf, cji);
    cfgdb->cjloaded++;
}


void cfgdbAddtoRecord(sysCfg* cfgdb,const char* field, const opendnp3::AnalogOutputInt32& cmd, uint16_t index)
{
    cJSON* cjf = cfgdbFindAddArray(cfgdb, field);
    cJSON* cji = cJSON_CreateObject();
    // todo resolve name
    cJSON_AddNumberToObject(cji,"offset",index);
    //const char* indexName = cfgindextoName(AOP32,index);
    //cJSON_AddStringToObject(cji,"offset",indexName);
    cJSON_AddNumberToObject(cji,"value", cmd.value);
    cJSON_AddItemToArray(cjf,cji);
    cfgdb->cjloaded++;
}

void cfgdbAddtoRecord(sysCfg* cfgdb,const char* field, const opendnp3::AnalogOutputFloat32& cmd, uint16_t index)
{
    cJSON* cjf = cfgdbFindAddArray(cfgdb, field);
    cJSON* cji = cJSON_CreateObject();
    // todo resolve name
    cJSON_AddNumberToObject(cji,"offset",index);
    //const char* indexName = cfgindextoName(Float32,index);
    //cJSON_AddStringToObject(cji,"offset",indexName);
    cJSON_AddNumberToObject(cji,"value", cmd.value);
    cJSON_AddItemToArray(cjf,cji);
    cfgdb->cjloaded++;
}

// TODO fix up CROB
void cfgdbAddtoRecord(sysCfg* cfgdb, const char* field, const char* cmd, uint16_t index)
{
    cJSON* cjf = cfgdbFindAddArray(cfgdb, field);
    cJSON* cji = cJSON_CreateObject();
    // todo resolve name
    cJSON_AddNumberToObject(cji,"offset",index);
    //const char* indexName = cfgindextoName(CROB,index);
    //cJSON_AddStringToObject(cji,"offset",indexName);
    // ??
    cJSON_AddStringToObject(cji,"value", cmd);
    cJSON_AddItemToArray(cjf, cji);
    cfgdb->cjloaded++;
}

// TODO allow a setup option in the config file to supply the SOEname
const char* cfgGetSOEName(sysCfg* cfgdb, const char* fname)
{
    return fname;
}

const ControlCode StringToControlCode(const char* codeWord)
{
    if (strncmp("NUL_CANCEL", codeWord, strlen("NUL_CANCEL")) == 0)
        return ControlCode::NUL_CANCEL;
    if (strncmp("NUL", codeWord, strlen("NUL")) == 0)
        return ControlCode::NUL;
    if (strncmp("PULSE_ON_CANCEL", codeWord, strlen("PULSE_ON_CANCEL")) == 0)
        return ControlCode::PULSE_ON_CANCEL;
    if (strncmp("PULSE_ON", codeWord, strlen("PULSE_ON")) == 0)
        return ControlCode::PULSE_ON;
    if (strncmp("PULSE_OFF_CANCEL", codeWord, strlen("PULSE_OFF_CANCEL")) == 0)
        return ControlCode::PULSE_OFF_CANCEL;
    if (strncmp("PULSE_OFF", codeWord, strlen("PULSE_OFF")) == 0)
        return ControlCode::PULSE_OFF;
    if (strncmp("LATCH_ON_CANCEL", codeWord, strlen("LATCH_ON_CANCEL")) == 0)
        return ControlCode::LATCH_ON_CANCEL;
    if (strncmp( "LATCH_ON", codeWord, strlen("LATCH_OFF")) == 0)
        return ControlCode::LATCH_ON;
    if (strncmp("LATCH_OFF_CANCEL", codeWord, strlen("LATCH_OFF_CANCEL")) == 0)
        return ControlCode::LATCH_OFF_CANCEL;
    if (strncmp( "LATCH_OFF", codeWord, strlen("LATCH_OFF")) == 0)
        return ControlCode::LATCH_OFF;
    if (strncmp("CLOSE_PULSE_ON_CANCEL", codeWord, strlen("CLOSE_PULSE_ON_CANCEL")) == 0)
        return ControlCode::CLOSE_PULSE_ON_CANCEL;
    if (strncmp("CLOSE_PULSE_ON", codeWord, strlen("CLOSE_PULSE_ON")) == 0)
        return ControlCode::CLOSE_PULSE_ON;
    if (strncmp("TRIP_PULSE_ON_CANCEL", codeWord, strlen("TRIP_PULSE_ON_CANCEL")) == 0)
        return ControlCode::TRIP_PULSE_ON_CANCEL;
    if (strncmp("TRIP_PULSE_ON", codeWord, strlen("TRIP_PULSE_ON")) == 0)
        return ControlCode::TRIP_PULSE_ON;
    return ControlCode::UNDEFINED;
}


bool process_dnp3_message(int bytes_read, int header_length, datalog* data, system_config* config, server_data* server_map, bool serial, uint8_t * query)
{
#if 0
    uint8_t  function = query[header_length];
    uint16_t offset = MODBUS_GET_INT16_FROM_INT8(query, header_length + 1);
    uint16_t num_regs = MODBUS_GET_INT16_FROM_INT8(query, header_length + 3);

    if(function == MODBUS_FC_WRITE_SINGLE_COIL)
    {
        if(bytes_read != header_length + (serial ? 7 : 5))
        {
            fprintf(stderr,"Modbus body length less than expected.\n");
            return false;
        }
        uint16_t value = num_regs;
        if( offset >= data[Coil].start_offset &&
            offset <  data[Coil].start_offset + data[Coil].num_regs)
        {
            offset -= data[Coil].start_offset;
            maps* reg = server_map->regs_to_map[Coil][offset];
            if(reg == NULL)
            {
                fprintf(stderr, "Wrote to undefined coil.\n");
                 return false;
            }
            cJSON* send_body = cJSON_CreateObject();
            if(send_body != NULL)
            {
                if(value == 0 || value == 0xFF00)
                {
                    char uri[512];
                    sprintf(uri, "%s/%s", reg->uri, reg->reg_name);
                    cJSON_AddBoolToObject(send_body, "value", (value == 0xFF00) ? cJSON_True : cJSON_False);
                    char* body_msg = cJSON_PrintUnformatted(send_body);
                    server_map->p_fims->Send("set", uri, NULL, body_msg);
                    free(body_msg);
                    cJSON_Delete(send_body);
                }
                else
                {
                    fprintf(stderr, "Invalid value sent to coil.\n");
                    cJSON_Delete(send_body);
                    return false;
                }
            }
        }
        // Let the modbus_reply handle error message
    }
    else if(function == MODBUS_FC_WRITE_MULTIPLE_COILS)
    {
        uint8_t data_bytes = query[header_length + 5];

        if(bytes_read != header_length + data_bytes + (serial ? 8 : 6) ||
           data_bytes != ((num_regs / 8) + ((num_regs % 8 > 0) ? 1 : 0)))
        {
            //error incorrect amount of data reported
            fprintf(stderr,"Modbus bytes of data does not match number of coils requested to write.\n");
            return false;
        }
        if( offset >= data[Coil].start_offset &&
            offset + num_regs <=  data[Coil].start_offset + data[Coil].num_regs)
        {
            offset -= data[Coil].start_offset;
            for(int i = 0 ; i < data_bytes; i++)
            {
                for(int j = 0; j < ((i != data_bytes -1) ? 8 : ((num_regs % 8 == 0) ? 8 : num_regs % 8)); j++)
                {
                    maps* reg = server_map->regs_to_map[Coil][(offset + i * 8 + j)];
                    if(reg == NULL)
                        //undefined coil
                        continue;
                    cJSON* send_body = cJSON_CreateObject();
                    if(send_body != NULL)
                    {
                        char uri[512];
                        sprintf(uri, "%s/%s", reg->uri, reg->reg_name);
                        uint8_t value = (query[header_length + 6 + i] >> j) & 0x01;
                        cJSON_AddBoolToObject(send_body, "value", (value == 1) ? cJSON_True : cJSON_False);
                        char* body_msg = cJSON_PrintUnformatted(send_body);
                        server_map->p_fims->Send("set", uri, NULL, body_msg);
                        free(body_msg);
                        cJSON_Delete(send_body);
                    }
                }
            }
        }
        // Let the modbus_reply handle error message
    }
    else if(function == MODBUS_FC_WRITE_SINGLE_REGISTER)
    {
        if(bytes_read != header_length + (serial ? 7 : 5))
        {
            fprintf(stderr,"Modbus body length less than expected, bytes read %d, header %d.\n", bytes_read, header_length);
            return false;
        }
        if( offset >= data[Holding_Register].start_offset &&
            offset <  data[Holding_Register].start_offset + data[Holding_Register].num_regs)
        {
            offset -= data[Holding_Register].start_offset;
            maps* reg = server_map->regs_to_map[Holding_Register][offset];
            if(reg == NULL)
            {
                fprintf(stderr, "Wrote to undefined register.\n");
            }
            else
            {
                if(reg->num_regs != 1)
                {
                    fprintf(stderr, "Wrote single register of Multi Register variable %s.\n", reg->reg_name);
                    //TODO either need to set the value or return error code
                }
                else
                {
                    cJSON* send_body = cJSON_CreateObject();
                    if(send_body != NULL)
                    {
                        char uri[512];
                        sprintf(uri, "%s/%s", reg->uri, reg->reg_name);

                        if(reg->is_bool)
                        {
                            bool temp_reg;
                            temp_reg = (num_regs == 1);
                            cJSON_AddBoolToObject(send_body, "value", temp_reg);
                        }
                        else
                        {
                            double temp_reg;
                            if(reg->sign == true)
                                temp_reg = static_cast<double>(static_cast<int16_t>(num_regs));
                            else
                                temp_reg = static_cast<double>(num_regs);

                            if(reg->scale != 0.0)
                                temp_reg /= reg->scale;
                            cJSON_AddNumberToObject(send_body, "value", temp_reg);
                        }
                        char* body_msg = cJSON_PrintUnformatted(send_body);
                        server_map->p_fims->Send("set", uri, NULL, body_msg);
                        free(body_msg);
                        cJSON_Delete(send_body);
                    }
                }
            }
        }
        // Let the modbus_reply handle error message
    }
    else if(function == MODBUS_FC_WRITE_MULTIPLE_REGISTERS)
    {
        uint8_t data_bytes =  query[header_length + 5];
        if(bytes_read != header_length + data_bytes + (serial ? 8 : 6) ||
           data_bytes != num_regs * 2)
        {
            fprintf(stderr,"Modbus bytes of data does not match number of holding registers requested to write.\n");
            return false;
        }
        if( offset >= data[Holding_Register].start_offset &&
            offset + num_regs <=  data[Holding_Register].start_offset + data[Holding_Register].num_regs)
        {
            offset -= data[Holding_Register].start_offset;
            for(unsigned int i = 0; i < num_regs;)
            {
                uint32_t byte_offset = header_length + 6 + i * 2;
                maps* reg = server_map->regs_to_map[Holding_Register][offset + i];
                if(reg == NULL)
                {
                    i++;
                    continue;
                }

                if(reg->reg_off != offset + i || reg->reg_off + reg->num_regs > offset + num_regs)
                {
                    fprintf(stderr, "Write does not include all registers of Multi Register variable %s.\n", reg->reg_name);
                    i++;
                    continue;
                }
                double temp_reg;
                if (reg->num_regs == 1)
                {
                    uint16_t temp_reg_int = MODBUS_GET_INT16_FROM_INT8(query, byte_offset);
                    // single register 16-bit short
                    if (reg->sign == true)
                        temp_reg = static_cast<double>(static_cast<int16_t>(temp_reg_int));
                    else
                        temp_reg = static_cast<double>(temp_reg_int);
                }
                else if(reg->num_regs == 2)
                {
                    //Combine 2 registers into 32-bit value
                    uint32_t temp_reg_int;
                    if (config->byte_swap == true)
                        temp_reg_int = ((static_cast<uint32_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset)    )      ) +
                                        (static_cast<uint32_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset + 2)) << 16) );
                    else
                    {
                        //TODO more debug is needed but byte order appears to be swapped on system so changing query
                        temp_reg_int = ((static_cast<uint32_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset)    )       ) +
                                        (static_cast<uint32_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset + 2)) << 16 ) );
                        query[byte_offset]     = static_cast<uint8_t>(temp_reg_int >> 24);
                        query[byte_offset + 1] = static_cast<uint8_t>((temp_reg_int & 0x00ff0000) >> 16);
                        query[byte_offset + 2] = static_cast<uint8_t>((temp_reg_int & 0x0000ff00) >> 8);
                        query[byte_offset + 3] = static_cast<uint8_t>(temp_reg_int & 0x000000ff);
                    }

                    if(reg->sign == true)
                        temp_reg = static_cast<double>(static_cast<int32_t>(temp_reg_int));
                    else
                        temp_reg = static_cast<double>(temp_reg_int);
                }
                else if(reg->num_regs == 4)
                {
                    uint64_t temp_reg_int;
                    if (config->byte_swap == true)
                        temp_reg_int = ((static_cast<uint64_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset)    )      ) +
                                        (static_cast<uint64_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset + 2)) << 16) +
                                        (static_cast<uint64_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset + 4)) << 32) +
                                        (static_cast<uint64_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset + 6)) << 48) );
                    else
                        temp_reg_int = ((static_cast<uint64_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset)    ) << 48) +
                                        (static_cast<uint64_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset + 2)) << 32) +
                                        (static_cast<uint64_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset + 4)) << 16) +
                                        (static_cast<uint64_t>(MODBUS_GET_INT16_FROM_INT8(query, byte_offset + 6))      ) );

                    if(reg->sign == true)
                        temp_reg = static_cast<double>(static_cast<int64_t>(temp_reg_int));
                    else
                        temp_reg = static_cast<double>(temp_reg_int);
                }
                else
                {
                    //not currently supported register count
                    fprintf(stderr, "Invalid Number of Registers defined for %s.\n", reg->reg_name);
                    i += reg->num_regs;
                    continue;
                }
                //Only want to scale if scale value is present and not == 0.0
                if (reg->scale != 0.0)
                    temp_reg /= reg->scale;

                cJSON* send_body = cJSON_CreateObject();
                if(send_body != NULL)
                {
                    char uri[512];
                    sprintf(uri, "%s/%s", reg->uri, reg->reg_name);
                    if(reg->is_bool)
                        cJSON_AddBoolToObject(send_body, "value", (temp_reg == 1));
                    else
                        cJSON_AddNumberToObject(send_body, "value", temp_reg);
                    char* body_msg = cJSON_PrintUnformatted(send_body);
                    server_map->p_fims->Send("set", uri, NULL, body_msg);
                    free(body_msg);
                    cJSON_Delete(send_body);
                }
                i += reg->num_regs;
            }
        }
        // Let the modbus_reply handle error message
    }
    // else no extra work to be done let standard reply handle
#endif
    
    return true;
}
