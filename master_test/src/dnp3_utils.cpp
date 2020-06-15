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
#include <climits>

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

// // modbus code 
// int establish_connection(system_config* config)
// {
//     struct ifreq ifr;
//     struct sockaddr_in addr;
//     int sock, yes;
//     // modbus listen only accepts on INADDR_ANY so need to make own socket and hand it over
//     sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
//     if (sock == -1)
//     {
//         fprintf(stderr, "Failed to create socket: %s.\n", strerror(errno));
//         return -1;
//     }

//     yes = 1;
//     if (-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &yes, sizeof(yes)))
//     {
//         fprintf(stderr, "Failed to set socket option reuse address: %s.\n", strerror(errno));
//         close(sock);
//         return -1;
//     }

//     memset(&addr, 0, sizeof(addr));
//     addr.sin_family = AF_INET;
//     //read port out of config
//     addr.sin_port = htons(config->port);

//     if(config->eth_dev != NULL)
//     {
//         ifr.ifr_addr.sa_family = AF_INET;
//         //read interface out of config file
//         strncpy(ifr.ifr_name, config->eth_dev, IFNAMSIZ - 1);
//         if(-1 == ioctl(sock, SIOCGIFADDR, &ifr))
//         {
//             fprintf(stderr, "Failed to get ip address for interface %s: %s.\n", ifr.ifr_name, strerror(errno));
//             close(sock);
//             return -1;
//         }
//         addr.sin_addr.s_addr = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
//     }
//     else if(config->ip_address != NULL)
//         addr.sin_addr.s_addr = inet_addr(config->ip_address);
//     else
//         addr.sin_addr.s_addr = INADDR_ANY;

//     if (-1 == bind(sock, (struct sockaddr *)&addr, sizeof(addr)))
//     {
//         fprintf(stderr, "Failed to bind socket: %s.\nIf trying to use a port less than 1024 run programs as sudo.\n", strerror(errno));
//         close(sock);
//         return -1;
//     }

//     if (-1 == listen(sock, 1))
//     {
//         fprintf(stderr, "Failed to listen on socket: %s.\n", strerror(errno));
//         close(sock);
//         return -1;
//     }

//     // Values passed to modbus_new_tcp are ignored since we are listening on our own socket
//     //config->mb = modbus_new_tcp("127.0.0.1", MODBUS_TCP_DEFAULT_PORT);
//     //if(config->mb == NULL)
//     //{
//     //    fprintf(stderr, "Failed to allocate context.\n");
//     //    close(sock);
//     //    return -1;
//     //}
//     //if(config->device_id != -1 && -1 == modbus_set_slave(config->mb, config->device_id))
//     //{
//     //    fprintf(stderr, "Valid Unit identifiers (Device_Id) are between 1-247.\n");
//     //    modbus_free(config->mb);
//     //    config->mb = NULL;
//     //    return -1;
//     //}
//     return sock;
// }
// uses unsigned int to extend the range for int
int32_t getInt32Val(DbVar *db)
{
    int32_t ival = static_cast<int32_t>(db->valuedouble); 
    if((db->sign == 0) && (db->valuedouble > INT_MAX) && (db->valuedouble <= UINT_MAX))
    {
        ival = -(db->valuedouble - INT_MAX);
    }
    return ival;
}
// uses unsigned int to extend the range for int
int16_t getInt16Val(DbVar *db)
{
    int16_t ival = static_cast<int16_t>(db->valuedouble); 
    if((db->sign == 0) && (db->valuedouble > SHRT_MAX) && (db->valuedouble <= USHRT_MAX))
    {
        ival = -(db->valuedouble - SHRT_MAX);
    }
    return ival;
}

bool extractInt32Val(double &dval, DbVar *db)
{
    bool flag = false;
    if(db->sign == 0)
    {
        flag = true;
        if  (db->valuedouble < 0.0)
        {
            dval = -(static_cast<double>(INT_MIN) + db->valuedouble);
        }
        else
        {
            dval = db->valuedouble;
        }        
    }
    return flag;
}

bool extractInt16Val(double &dval, DbVar *db)
{
    bool flag = false;
    if((db->sign == 0) && (db->valuedouble < 0))
    {
        dval = -(SHRT_MIN + db->valuedouble);
        flag = true;
    }
    return flag;
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

DbVar* getDbVar(sysCfg *sysdb, const char * uri, const char *name)
{
    return sysdb->getDbVar(uri, name);
}

//TODO check remove old version deprecated
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
                snprintf(tmp,1024,"/%s/%s/%s/%s", "id", "components", ev, sys->id);
            }
            else
            {
                snprintf(tmp,1024,"/%s/%s/%s", "id", "components", sys->id);
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

void addCjVal(cJSON* cj, const char* dname, int flag, double val)
{
    if(flag & PRINT_VALUE)
    {
        cJSON* cji = cJSON_CreateObject();
        cJSON_AddNumberToObject(cji, "value", val);
        cJSON_AddItemToObject(cj, dname, cji);
    }
    else
    {
        cJSON_AddNumberToObject(cj, dname, val);
    }
}

void addCjVal(cJSON* cj, const char* dname, int flag, uint32_t val)
{
    if(flag & PRINT_VALUE)
    {
        cJSON* cji = cJSON_CreateObject();
        cJSON_AddNumberToObject(cji, "value", val);
        cJSON_AddItemToObject(cj, dname, cji);
    }
    else
    {
        cJSON_AddNumberToObject(cj, dname, val);
    }
}

void addCjVal(cJSON* cj, const char* dname, int flag, int val)
{
    if(flag & PRINT_VALUE)
    {
        cJSON* cji = cJSON_CreateObject();
        cJSON_AddNumberToObject(cji, "value", val);
        cJSON_AddItemToObject(cj, dname, cji);
    }
    else
    {
        cJSON_AddNumberToObject(cj, dname, val);
    }
}

void addCjVal(cJSON* cj, const char* dname, int flag, const char* val)
{
    if(flag & PRINT_VALUE)
    {
        cJSON* cji = cJSON_CreateObject();
        cJSON_AddStringToObject(cji, "value", val);
        cJSON_AddItemToObject(cj, dname, cji);
    }
    else
    {
        cJSON_AddStringToObject(cj, dname, val);
    }
}

// This is the root function
int addVarToCj(cJSON* cj, DbVar* db, int flag)
{
    int rc = 0;
    // always use the value from the actual compoent 
    const char* dname = db->name.c_str();
    if((flag & PRINT_PARENT) &&  (db->parent != NULL))
        dname = db->parent->name.c_str();
    switch (db->type)
    {
        case Type_AnalogOS:
        case Type_Analog:
        {
            if(db->variation == Group30Var5)
                addCjVal(cj, dname, flag, db->valuedouble);
            else
            {
                if(db->sign)
                {
                    addCjVal(cj, dname, flag, static_cast<double>(static_cast<int32_t>(db->valuedouble)));
                }
                else
                {
                    addCjVal(cj, dname, flag, db->valuedouble);        
                }
            }

            break;
        }

        case Type_BinaryOS:
        case Type_Binary:
        {
            addCjVal(cj, dname, flag, static_cast<int32_t>(db->valuedouble));
            break;
        }
        case Type_Crob:
        {
            FPS_DEBUG_PRINT("*** %s Found variable [%s] type  %d crob %u [%s] \n"
                    , __FUNCTION__, dname, db->type, db->crob, ControlCodeToString(TypeToControlCode(db->crob)));
            addCjVal(cj, dname, flag, ControlCodeToString(TypeToControlCode(db->crob)));
            // if(flag & PRINT_VALUE)
            // {
            //     cJSON* cji = cJSON_CreateObject();
            //     cJSON_AddStringToObject(cji, "value", ControlCodeToString(TypeToControlCode(db->crob)));
            //     cJSON_AddItemToObject(cj, dname, cji);
            // }
            // else
            // {
            //     cJSON_AddStringToObject(cj, dname, ControlCodeToString(TypeToControlCode(db->crob)));
            // }
            break;
        }
        case AnIn16:
        {
            double dval = 0.0;
            if (extractInt16Val(dval, db) == true)
            {
                // this can still be an int
                addCjVal(cj, dname, flag, static_cast<int32_t>(dval));
            }
            else
            {
                addCjVal(cj, dname, flag, static_cast<int32_t>(db->valuedouble));                
            }
            break;
        }
        case AnIn32:
        {
            double dval = 0.0;
            if (extractInt32Val(dval, db) == true)
            {                
                FPS_ERROR_PRINT("*** %s Found variable [%s] type  %d sign %d usng dval %f \n"
                                , __FUNCTION__, db->name.c_str(), db->type, db->sign, dval);
                addCjVal(cj, dname, flag, dval);
            }
            else
            {
                FPS_ERROR_PRINT("*** %s Found variable [%s] type  %d sign %d using valuedouble %f\n"
                                , __FUNCTION__, db->name.c_str(), db->type, db->sign, db->valuedouble);
                addCjVal(cj, dname, flag, db->valuedouble);                
            }            
            break;
        }
        case AnF32:
        {
            addCjVal(cj, dname, flag, db->valuedouble);
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

int addVarToCj(sysCfg* sys, cJSON* cj, const char *uri, const char* dname)
{
    DbVar* db = sys->getDbVar(uri, dname);

    return addVarToCj(cj, db, 0);
}
// these are the types decoded from the config file
// must match the typedef sequence
const char* dreg_types[] = { "AnOPInt16", "AnOPInt32", "AnOPF32", "CROB", "analog", "binary", 0 };

const char *iotypToStr (int t)
{
//    if (t < Type_of_Var::NumTypes)
    if (t < static_cast<int32_t> (sizeof(dreg_types))/static_cast<int32_t>(sizeof(dreg_types[0])))
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

int variation_decode(const char* ivar)
{
    if(ivar)
    {
        if (strcmp(ivar, "Group30Var5") == 0)
            return Group30Var5;
        else if (strcmp(ivar, "Group30Var2") == 0)
            return Group30Var2;
        else if (strcmp(ivar, "Group32Var7") == 0)
            return Group32Var7;
    }
    return GroupUndef;
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

bool getCJcj (cJSON *cj, const char *name, cJSON*& val, bool required)
{
    bool ok = !required;
    cJSON *cji = cJSON_GetObjectItem(cj, name);
    if (cji) {
        if(val) cJSON_Delete(val);
        val = cJSON_Duplicate(cji->child, true);
        ok = true;
    }
    return ok;
}

// modbus system example
// "notes":" a collection of inputs from the system all using default uri",
// 		"id": "sel_735",
// 		"ip_address": "127.0.0.1",
// 		"port": 12502,
// 		"ip_address2": "192.168.0.136",
// 		"port2": 502,
// 		"frequency": 500,
// 		"byte_swap": false

bool parse_system(cJSON* cji, sysCfg* sys, int who)
{
    bool ret = true;
    cJSON *cj = cJSON_GetObjectItem(cji, "system");

    if (cj == NULL)
    {
        FPS_ERROR_PRINT("system  missing from file! \n");
        ret = false;
    }
    if(who == DNP3_MASTER )
    {
        sys->local_address = 1;
        sys->remote_address = 10;
    }
    else
    {
        sys->local_address = 10;
        sys->remote_address = 1;
    }
    
    // todo add different frequencies for each zone
    sys->frequency  = 1000; // default once a second
    if(ret) ret = getCJint(cj,"version",         sys->version,        false );
    if(ret) ret = getCJint(cj,"frequency",       sys->frequency,      false);
    if(ret) ret = getCJint(cj,"port",            sys->port,           true);
    if(ret) ret = getCJint(cj,"local_address",   sys->local_address,  false);
    if(ret) ret = getCJint(cj,"remote_address",  sys->remote_address, false);
    if(ret) ret = getCJstr(cj,"id",              sys->id,             true);
    if(ret) ret = getCJstr(cj,"protocol",        sys->protocol,       false);
    if(ret) ret = getCJstr(cj,"ip_address",      sys->ip_address,     true);
    //if(ret) ret = getCJstr(cj,"pub",             sys->pub,            false);
    if(ret) ret = getCJstr(cj,"name",            sys->name,           false);
    if(ret) ret = getCJint(cj,"debug",           sys->debug,          false);
    //TODO use this
    if(ret) ret = getCJstr(cj,"base_uri",         sys->base_uri,      false);

    // fixup base_uri
    char tmp[1024];
    const char* sys_id = sys->id;
    if(sys->base_uri)
    {
        snprintf(tmp, sizeof(tmp),"%s/%s", sys->base_uri, sys_id);
        free((void *) sys->base_uri);
    }
    else
    {
        if (who == DNP3_MASTER)
        {
            snprintf(tmp, sizeof(tmp),"%s/%s", "/components", sys_id);
        }
        else
        {
            snprintf(tmp, sizeof(tmp),"%s/%s", "/interfaces", sys_id);
        }
    }
    sys->base_uri = strdup(tmp);

    return ret;
}



// big wrinkle here.. Lets parse the modbus file  
// "registers": 
// 	[
// 		{
// 			"type": "Input Registers",
// 			"dnp3_type": "Analog",
// 			"starting_offset": 350,
// 			"number_of_registers": 26,
// 			"map":
// 			[
// 				{
// 					"id": "current_a",
// 					"offset": 350,
// 					"size": 2,
// 					"scale":100,
// 					"signed": true
//               }
//          ]
//      }
//  ]

// parse a map of items
int parse_items(sysCfg* sys, cJSON* objs, int idx, int who)
{
    cJSON* obj;
    cJSON_ArrayForEach(obj, objs)
    {
        cJSON *id, *offset, *uri, *bf, *bits, *variation;
        cJSON *evariation, *linkback, *clazz, *rsize, *sign;
        cJSON *scale, *cjidx;

        if(obj == NULL)
        {
            FPS_ERROR_PRINT("Invalid or NULL obj\n");
            continue;
        }

        // note that id translates to name
        id         = cJSON_GetObjectItem(obj, "id");
        cjidx      = cJSON_GetObjectItem(obj, "idx");
        offset     = cJSON_GetObjectItem(obj, "offset");
        rsize      = cJSON_GetObjectItem(obj, "size");
        variation  = cJSON_GetObjectItem(obj, "variation");
        evariation = cJSON_GetObjectItem(obj, "evariation");
        uri        = cJSON_GetObjectItem(obj, "uri");
        bf         = cJSON_GetObjectItem(obj, "bit_field");
        bits       = cJSON_GetObjectItem(obj, "bit_strings");
        linkback   = cJSON_GetObjectItem(obj, "linkback");
        clazz      = cJSON_GetObjectItem(obj, "clazz");
        sign       = cJSON_GetObjectItem(obj, "signed");
        scale      = cJSON_GetObjectItem(obj, "scale");
        

        if (id == NULL || offset == NULL || id->valuestring == NULL)
        {
            FPS_ERROR_PRINT("NULL variables or component_id \n");
            continue;
        }
        // allow 32 bit systems ints
        // TODO rework this
        int myidx = idx;
        if (rsize != NULL) 
        {
            if (idx == AnIn16)
            {
                if (rsize->valueint > 1)
                {
                    myidx = AnIn32; 
                }
            } 
        }

        // rework
        // DbVars are sorted by uri's now.
        // we can have the same DbVar name for different uris.
        // incoming messages from master only know type (AnIn16,AnIn32,AF32,CROB) and offset.
        // for these items offset is still set by the config file.
        // The return path from outstation to master still has type an index. The system has changed to use the offset from the conig file here too.
        // so we can no longer use the vector position as an index.
        // items are added from the config file against a registered uri ot the default one.
        // pubs, gets and sets from FIMS ues a combination of URI + var name to find vars.
        // data incoming from master to outstation will use type and offset to find the variable.
        // each type will have a map <int,DbVar *> to search
        // Data base for the outstation vars (analog and binary) will no longer use the vector size but max offset as db size ????
        // the <int , dBVar *> map can also  be used for these vars
        //
        // Get this going as follows 
        //   1.. parse config file using the uri's.
        //   2.. get the uri/var name search  working for FIMS querues
        //   3.. get the int, DbVar* search / map  running to find vars for outstation builder.
        //   4.. get the  mapping working for the incoming commands.
        //    
        // TODO No central map any more.
        // 

        // the cjidx fielcs will ovride the auto idx.

        DbVar* db = sys->newDbVar(id->valuestring, myidx, offset->valueint, uri?uri->valuestring:NULL, variation?variation->valuestring:NULL);

        if(evariation &&(db != NULL))
        {
            db->setEvar(evariation->valuestring);
        }
        if(clazz &&(db != NULL))
        {
            db->setClazz(clazz->valueint);
            FPS_ERROR_PRINT("****** variable [%s] set to clazz %d\n", db->name.c_str(), db->clazz);
        }

        if(sign &&(db != NULL))
        {
            db->sign = (sign->type == cJSON_True);
            FPS_ERROR_PRINT("****** variable [%s] set to signed %d\n", db->name.c_str(), db->sign);
        }
        
        if(scale &&(db != NULL))
        {
            db->scale = (scale->valueint);
            FPS_ERROR_PRINT("****** variable [%s] scale set %d\n", db->name.c_str(), db->sign);
        }

        if (bf && bits && (bits->type == cJSON_Array))
        {
            //TODO bits need a value use make_pair there too 
            FPS_ERROR_PRINT("*****************Adding bitfields for %s\n", db->name.c_str());
            sys->addBits(db, bits);

        }

        FPS_DEBUG_PRINT(" config adding name [%s] id [%d]\n", id->valuestring, offset->valueint);
        // keep a count of the objects.
        sys->numObjs[idx]++; 

        // set up uri
        char* nuri = sys->getDefUri(who);
        if (uri && uri->valuestring) 
        {
            nuri = uri->valuestring;
        }
        if(cjidx)
        {
            db->idx = cjidx->valueint;
        }
        sys->setDbIdxMap(db);

        // deprecated
        sys->addUri(nuri, db);

        // new way 
        sys->addDbUri(nuri, db);

        // Deal with linkback option ( which replaces readback function)
        // the master SOEhandler will cause the linkback value to be updated.
        // we may mirror this in the outstation handler too.  
        // master and outstation have different linkback types
        if((idx == Type_Analog) || (idx == Type_Binary)) 
        {
            if (who == DNP3_MASTER)
            {
                if(linkback)
                {
                    // copy the variable name into the linkback string.
                    // char* linkback; 
                    // DbVar* linkb
                    // the variable may not exist yet so it will be found later.
                    if (linkback->valuestring != NULL)
                    {
                        FPS_ERROR_PRINT(" Setting linkback variable name to [%s]\n", linkback->valuestring);
                        db->linkback = strdup(linkback->valuestring);
                        db->linkb = sys->getDbVar(nuri, linkback->valuestring);
                    }
                }
            }
        }
        else
        {
            if (who == DNP3_OUTSTATION)
            {
                if(linkback)
                {
                    // copy the variable name into the linkback string.
                    // char* linkback; 
                    // DbVar* linkb
                    // the variable may not exist yet so it will be found later.
                    if (linkback->valuestring != NULL)
                    {
                        FPS_ERROR_PRINT(" Setting linkback variable name to [%s]\n", linkback->valuestring);
                        db->linkback = strdup(linkback->valuestring);
                        db->linkb = sys->getDbVar(nuri, linkback->valuestring);
                    }
                }
            }
        }
        //if we see a readback, decode the readback type and add that to the status vars
        // if readbidx == analog then readbtype =  Type_AnalogOS
        //db = sys->newDbVar(id->valuestring+"_OS", readbtype, offset->valueint, uri?uri->valuestring:NULL, NULL);//variation?variation->valuestring:NULL);
    }
    return  sys->numObjs[idx]; 
}

int  parse_object(sysCfg* sys, cJSON* objs, int idx, int who)
{
    cJSON* cjlist = cJSON_GetObjectItem(objs, iotypToStr(idx));
    if (cjlist == NULL)
    {
        FPS_ERROR_PRINT("[%s] objects missing from config! \n",iotypToStr(idx));
        return -1;
    }
    return parse_items(sys, cjlist, idx, who);
}
// "registers": 
// 	[
// 		{
// 			"type": "Input Registers",
// 			"dnp3_type": "Analog",
// 			"starting_offset": 350,
// 			"number_of_registers": 26,
// 			"map":
// 			[
// 				{
// 					"id": "current_a",
// 					"offset": 350,
// 					"size": 2,
// 					"scale":100,
// 					"signed": true
//               }
//          ]
//      }
//  ]

bool parse_modbus(cJSON* cj, sysCfg* sys, int who)
{
    // config file has "objects" with children groups "binary" and "analog"
    // who is needd to stop cross referencing linkvars
    cJSON* cji;
    cJSON_ArrayForEach(cji, cj)
    {
        cJSON* cjmap = cJSON_GetObjectItem(cji, "map");
        cJSON* cjtype = cJSON_GetObjectItem(cji, "dnp3_type");
        // dnp3_type can be output, valuedouble holds it all 
        if ((cjmap == NULL) || (cjmap->type != cJSON_Array))
        {
            FPS_ERROR_PRINT("modbus registers map object is not an array ! \n");
            return false;
        }
        if ((cjtype == NULL) || (cjtype->type != cJSON_String))
        {
            FPS_ERROR_PRINT("modbus registers dnp3_type missing or  object is not an string ! \n");
            return false;
        }
        int idx = iotypToId (cjtype->valuestring);
        if (idx < 0)
        {
            FPS_ERROR_PRINT("modbus dnp3_type [%s] not recognised! \n", cjtype->valuestring);
            return false;
        }
        parse_items(sys, cjmap, idx, who);
    }
    return true;
}

bool parse_variables(cJSON* object, sysCfg* sys, int who)
{
    for (int idx = 0; idx< Type_of_Var::NumTypes; idx++)
        sys->numObjs[idx] = 0;

    // config file has "objects" with children groups "binary" and "analog"
    // who is needd to stop cross referencing linkvars
    cJSON* cjregs = cJSON_GetObjectItem(object, "registers");
    if (cjregs != NULL)
    {
        return parse_modbus(cjregs, sys, who);
    }

    cJSON *JSON_objects = cJSON_GetObjectItem(object, "objects");
    if (JSON_objects == NULL)
    {
        FPS_ERROR_PRINT("objects object is missing from file! \n");
        return false;
    }
    for (int idx = 0; idx< Type_of_Var::NumTypes; idx++)
        parse_object(sys, JSON_objects, idx, who);
    return true;
}

int getSysUris(sysCfg* sys, int who, const char **&subs, bool *&bpubs)
{
    int num = sys->getSubs(NULL, 0, who);
    subs = (const char **) malloc((num+3) * sizeof(char *));
    if(subs == NULL)
    {
        FPS_ERROR_PRINT("Failed to creae subs array.\n");
        //rc = 1;
        return -1;
    }
    bpubs = (bool *) malloc((num+3) * sizeof(bool));
    memset(bpubs, false, (num+3) * sizeof(bool)); // all false we hope

    num = sys->getSubs(subs, num, who);
    // for (int i = 0; i < snum; i++)
    // {
    //     subs[num++] = slogs[i];
    // }
    return num;
}

bool checkWho(sysCfg*sys, DbVar* db, int who)
{
    if(db == NULL) return false;
    if (who == DNP3_OUTSTATION)
    {
       if((db->type == Type_Analog ) || (db->type == Type_Binary)) return true;
       // if we have a readb registers then we can set it
       if(db->readb != NULL) return true;
    }
    else
    {
       if((db->type == AnIn16 ) 
            || (db->type == AnIn32)
            || (db->type == AnF32)
            || (db->type == Type_Crob)
            ) return true;
    }
    return false;    
}

bool checkWho(sysCfg*sys, const char* uri, const char *name, int who)
{
    DbVar* db = sys->getDbVar(uri, name);
    return checkWho(sys, db, who);
}
//std::vector<std::pair<DbVar*,int>>dbs; // collect all the parsed vars here
cJSON* parseValues(dbs_type& dbs, sysCfg*sys, fims_message*msg, int who, cJSON* body_JSON)
{
    cJSON* itypeValues = body_JSON;
    cJSON* cjit = NULL;
    if(sys->debug == 1)
        FPS_DEBUG_PRINT("Found variable list or array \n");
    // decode values may be in an array , TODO arrays are DEPRECATED
    if (cJSON_IsArray(itypeValues)) 
    {
        if(sys->debug == 1)
            FPS_DEBUG_PRINT("Found array of variables  \n");

        cJSON_ArrayForEach(cjit, itypeValues) 
        {
            cJSON* cjo = cJSON_GetObjectItem(cjit, "offset");
            cJSON* cjv = cJSON_GetObjectItem(cjit, "value");
            addValueToVec(dbs, sys, msg, cjo->valuestring, cjv, 0);
        }
    }
    else
    {
        // process a simple list
        cjit = itypeValues->child;
        if(sys->debug == 1)
            FPS_DEBUG_PRINT("****** Start with variable list iterator->type %d\n\n", cjit->type);
        //char* curi = strdup(msg->uri);
        //char* mcuri = NULL;
        while(cjit != NULL)
        {
            int flag = 0;
            if(sys->debug == 1)
                FPS_DEBUG_PRINT("Found variable name  [%s] child %p \n"
                                        , cjit->string
                                        , (void *)cjit->child
                                        );
            if (!checkWho(sys, msg->uri, cjit->string, who))
            {
                if(sys->debug == 1)
                    FPS_DEBUG_PRINT("variable [%s] NOT set ON %d\n"
                                    , cjit->string
                                    , who
                                    );
            }
            else
            {
                addValueToVec(dbs, sys, msg, cjit->string, cjit, flag);
            }
            cjit = cjit->next;
        }
        if(sys->debug == 1)
            FPS_DEBUG_PRINT("***** Done with variable list \n\n");
    }
    return body_JSON;//return dbs.size();
}



// uri handling
// we create a list of uris from cofig data
// a var name can be partly a uri
//   gateway/system1/voltageSP
// with a uri /components/fleetmanager/
// this incoming uri in the fims request will be
// get  /components/fleetmanager/[gateway/system1/voltageSP]
// anything past /components/fleetmanager/  must be a name simple
// but what do we forward to /components/gateway1
// so we have a set uri , used for pubs
// and a get uri used to pick up pubs etc.
// so the test checkUri makes sure we have a name / uri match
//  
// si  
int countUris(const char* uri)
{
    int nfrags = 0;
    for (int i = 0; i < (int)strlen(uri); i++ )
    {
        if (uri[i] == '/')
            nfrags++;
    }
    return nfrags;
}

// we have an incoming uri
//std::vector<std::pair<DbVar*,int>>dbs; // collect all the parsed vars here
cJSON* parseBody(dbs_type& dbs, sysCfg*sys, fims_message*msg, int who)
{
    //const char* uri = NULL;
    const char* dburi = NULL;
    int fragptr = 1;
    int single = 0;
    cJSON* body_JSON = cJSON_Parse(msg->body);
    cJSON* itypeA16 = NULL;
    cJSON* itypeA32 = NULL;
    cJSON* itypeF32 = NULL;
    cJSON* itypeValues = NULL;
    cJSON* itypeCROB = NULL;
    //cJSON* cjit = NULL;
    int reffrags = 0; // nfrags in the reference uri 
    DbVar* db = NULL;

    // msg->nfrags number of frags in the message
    // if reffrags == nfrags we have no name
    // if msg->nfrags - reffrags == 1 and we find a var for msg->pfrags[reffrags]; we have a single
    // 
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
    if(sys->debug == 1)
        FPS_ERROR_PRINT("fims message uri [%s] test for enough pfrags [%d] id [%s] \n", msg->uri, msg->nfrags, sys->id);
    
    if (msg->nfrags < 2)
    {
        FPS_ERROR_PRINT("fims message uri [%s] not enough pfrags [%d] id [%s] \n", msg->uri, msg->nfrags, sys->id);
        return body_JSON;
    }
    // special case [/interfaces/hybridos/reply/interfaces/hybridos] 
    // valid uri
    // /assets/feeders/feed_6
    // 
    // possible inputs
    // /assets/feeders/feed_6/feeder_kW_slew_rate  ==> process single  
    // /assets/feeders/feed_6                      ==> process list
    // /assets/feeders/feed_6/test/feeder_kW_slew_rate    == reject unless we have a var called "test/feeder_kW_slew_rate"
    //
    bool uriOK = sys->confirmUri(NULL, msg->uri, reffrags);
    bool isReply = false;
    if(sys->debug == 1)
        FPS_ERROR_PRINT("fims message first test msg->uri [%s]  uriOK %d nfags %d reffrags %d\n", msg->uri, uriOK, msg->nfrags, reffrags);

    if((strstr(msg->uri, "/reply/") != NULL) && (strstr(msg->uri, sys->id) != NULL))
    {
        if(sys->debug == 1)
            FPS_ERROR_PRINT("fims message msg->uri [%s] reply uri [%s] ACCEPTED \n", msg->uri, sys->id);
        reffrags = msg->nfrags;
        isReply = true;
    }

    // may be a single but we have to find the var
    single = 0;

    if(isReply == false)
    {
        char* curi = strdup(msg->uri);
        char* mcuri = NULL;

        if (uriOK == false)
        {
            FPS_ERROR_PRINT("fims message msg->uri [%s] frag [%s] Not ACCEPTED \n", msg->uri, sys->id);
            return body_JSON;
        }
        if((int)msg->nfrags > reffrags)
        {
            dburi = msg->pfrags[reffrags];
            mcuri = strstr(curi, dburi);
            if(mcuri != NULL)
            {
                // force a termination at the '/'
                mcuri[-1] = 0;
            }

            db = sys->getDbVar(curi, dburi);
            if(db == NULL)
            {        
                FPS_ERROR_PRINT("fims message msg->uri [%s] name  [%s] Not Found  sysid [%s] \n", msg->uri, dburi, sys->id);
                free((void*)curi);
                return body_JSON;
            }
            single = 1;
        }
        int urifrags = 0;
        if(sys->debug == 1)
            FPS_ERROR_PRINT(" %s Running with uri: [%s] single %d  \n", __FUNCTION__, dburi, single);
        uriOK = sys->confirmUri(db, msg->uri, urifrags);
        if(sys->debug == 1)
            FPS_ERROR_PRINT("   RECHECK fims message uri [%s] on  [%d]/[%s] uriOK is %d uriflags [%d]\n", dburi, who, sys->id, uriOK, urifrags);

        if(uriOK == false)
        {
            
            FPS_ERROR_PRINT("fims message frag %d [%s] not for this %d [%s] and uriOK is %d \n", fragptr+1, dburi, who, sys->id, uriOK);
            free((void*)curi);
            return body_JSON;
        }   
        if((strcmp(msg->method,"set") != 0) && (strcmp(msg->method,"get") != 0) && (strcmp(msg->method,"pub") != 0) && (strcmp(msg->method,"post") != 0))
        {
            FPS_ERROR_PRINT("fims unsupported method [%s] \n", msg->method);
            free((void*)curi);
            return body_JSON;
        }
    }
    // this is the "debug" or development method
    //-m set -u "/components/<some_master_id>/[<some_outstation_id>] '{"AnalogInt32": [{"index":<index>,"value":52},{"index":2,"value":5}]}' 

    // get is OK codewise..
    // TODO remove  readb vars 
    // find a good looking uri and find the number of frags
    //  
    if(strcmp(msg->method, "get") == 0)
    {
        

        int flag = 0;
        if(sys->debug == 1)
            FPS_ERROR_PRINT("fims method [%s] almost  supported for [%d]\n", msg->method, who);
        if(single ==1)
        {
            if(sys->debug == 1)
                FPS_DEBUG_PRINT("Found SINGLE variable [%s] type  %d \n", db->name.c_str(), db->type); 
            dbs.push_back(std::make_pair(db, flag));
            return body_JSON;
        }
        // else get them all
        // but only the ones associated with the uri
        else
        {
            char* curi = strdup(msg->uri);
            
            sys->addVarsToVec(dbs, curi);
            free((void*)curi);
            return body_JSON;
        }
        return body_JSON;
    }

    // TODO check this out and clean it up
    // Allow "pub" to "set" outstation
    if(strcmp(msg->method,"set") == 0 || (strcmp(msg->method,"pub") == 0 && (who == DNP3_OUTSTATION)))
    {
        //TODO need to ignore sets on the outstation for master vars and vice versa 
        //int single = 0;
        //int reply = 1;
        // watch out for sets on /interfaces/outstation/dnp3_outstation/reply/dnp3_outstation
        // handle a single item set getting better 
        //if(sys->debug == 1)
        if(single == 1)
        {
            if (db != NULL)
            {
                if(sys->debug == 1)
                {
                    FPS_ERROR_PRINT("fims method [%s] uri [%s]  supported on [%d] single %d name [%s]\n"
                                        , msg->method
                                        , msg->uri
                                        , who
                                        , single
                                        , db->name.c_str()
                                        );
                }
            }
            else
            {
                FPS_ERROR_PRINT("fims method [%s] uri [%s]  supported on [%d] NO DBVAR\n"
                                    , msg->method
                                    , msg->uri
                                    , who
                                    );
                return body_JSON;
            }
        }
        
        if(strstr(msg->uri, "/reply/") != NULL)
        {
             FPS_DEBUG_PRINT("fims message reply uri DETECTED  [%s] \n", msg->uri);
             single = 0;
        }

        //uri = msg->pfrags[fragptr+2];  // TODO check for delim. //components/master/dnp3_outstation/line_voltage/stuff
        // look for '{"debug":"on"/"off"}' or '{"scan":1,2 or 3} {"unsol": true or false} {"class" '{"<varname>":newclass}}
        if(strstr(msg->uri, "/_system") != NULL)
        {
            FPS_DEBUG_PRINT("fims system command [%s] body [%s]\n", msg->uri, msg->body);
            cJSON* cjsys = cJSON_GetObjectItem(body_JSON, "debug");
            if (cjsys != NULL)
            {
                if(strcmp(cjsys->valuestring,"on")== 0)
                    sys->debug = 1;
                if(strcmp(cjsys->valuestring,"off")== 0)
                    sys->debug = 0;
            }
            cjsys = cJSON_GetObjectItem(body_JSON, "scan");
            if (cjsys != NULL)
            {
                sys->scanreq = cjsys->valueint;
            }
            cjsys = cJSON_GetObjectItem(body_JSON, "unsol");
            if (cjsys != NULL)
            {
                sys->unsol = cjsys->valueint;
            }
            cjsys = cJSON_GetObjectItem(body_JSON, "class");
            if (cjsys != NULL)
            {
                sys->cjclass = cjsys;
            }
            return body_JSON;

        }
            // TODO redundant
        if(strstr(msg->uri, "/reply/") != NULL)
        {
            //if(sys->debug == 1)
                FPS_DEBUG_PRINT("fims message reply uri ACCEPTED  [%s] \n", msg->body);
            single = 0;
        }            
        

        if(single == 1)
        {
            // process a single var
            if(sys->debug == 1)
                FPS_ERROR_PRINT("Found variable [%s] type  %d run set %d\n"
                                        , db->name.c_str()
                                        , db->type
                                        , who
                                        );

            
            int flag = 0;
            if(sys->debug == 1)
                FPS_DEBUG_PRINT("Found variable type  %d \n", db->type);
            itypeValues = body_JSON;
            // allow '"string"' OR '{"value":"string"}'
            if(itypeValues->type == cJSON_Object)
            {
                flag |= PRINT_VALUE;
                itypeValues = cJSON_GetObjectItem(itypeValues, "value");
            }
            if(sys->debug == 1)
            {
                FPS_ERROR_PRINT("Found variable [%s] type  %d  sign %d body [%s]  flag %d \n"
                                        , db->name.c_str()
                                        , db->type
                                        , db->sign
                                        , msg->body
                                        , flag);
            }
            // Only Crob gets a string 
            if(itypeValues)
            {
                if (itypeValues->type == cJSON_String)
                {
                    if(db->type == Type_Crob)
                    {
                        uint8_t cval = ControlCodeToType(StringToControlCode(itypeValues->valuestring));
                        sys->setDbVarIx(Type_Crob, db->idx, cval);
                        db->initSet = 1;
                        // send the readback
                        dbs.push_back(std::make_pair(db, flag));
                        FPS_DEBUG_PRINT(" ***** %s Adding Direct CROB value %s offset %d idx %d uint8 cval 0x%02x\n"
                                        , __FUNCTION__, itypeValues->valuestring, db->offset, db->idx
                                        , cval
                                        );
                    }
                // TODO any other strings
                // do we have to convert strings into numbers ??
                }
                else
                {
                    // handle readb
                    //if(sys->useReadb[db->type] && (db->readb != NULL))
                    if(db->readb != NULL)
                    {
                        FPS_ERROR_PRINT(" ***** %s using readb (%s) as [%s] for a set to [%s]\n"
                                        , __FUNCTION__
                                        , sys->useReadb[db->type]?"true":"false"
                                        , db->readb->name.c_str()
                                        , db->name.c_str()
                                        );
                        if(sys->useReadb[db->type])
                        {
                            flag |= PRINT_PARENT;
                            db = db->readb;
                        }
                    }
                    sys->setDbVar(db, itypeValues);
                    dbs.push_back(std::make_pair(db, flag));
                }     
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
            return parseValues(dbs, sys, msg, who, body_JSON);
        }
    }
    return body_JSON;//return dbs.size();
}

// TODO need no dbs option
// TODO handle readb code
// add who to sys
int addValueToVec(dbs_type& dbs, sysCfg*sys, fims_message* msg, const char* name, cJSON *cjvalue, int flag)
{
    // cjoffset must be a name
    // cjvalue may be an object
    // msg->uri // just take it up to the name
    char* curi = strdup(msg->uri);
    char* mcuri = strstr(curi, name);
    if(mcuri != NULL)
    {
        // force a termination at the '/'
        mcuri[-1] = 0;
    }

    if (name == NULL)
    {
        FPS_ERROR_PRINT(" ************** %s offset is not  string\n",__FUNCTION__);
        free((void*)curi);
        return -1;
    }

    DbVar* db = getDbVar(sys, (const char*)curi, name); 
    if (db == NULL)
    {
        FPS_ERROR_PRINT( " ************* %s Var [%s] not found in dbMap\n", __FUNCTION__, name);
        free((void*)curi);
        return -1;
    }
    
    if(sys->debug == 1)
        FPS_DEBUG_PRINT(" ************* %s Var [%s] found in dbMap\n", __FUNCTION__, name);

    if (cjvalue->type == cJSON_Object)
    {
        flag |= PRINT_VALUE;
        if(sys->debug == 1)
            FPS_DEBUG_PRINT(" ************* %s Var [%s] set flag to 1 \n", __FUNCTION__, name);
        cjvalue = cJSON_GetObjectItem(cjvalue, "value");
    }

    if (!cjvalue)
    {
        if(sys->debug == 1)
            FPS_ERROR_PRINT(" ************** %s value not correct\n",__FUNCTION__);
        free((void*)curi);
        return -1;
    }

    if (db->type == Type_Crob) 
    {
        if(sys->debug == 1)
            FPS_DEBUG_PRINT(" ************* %s Var [%s] CROB setting value [%s]  to %d \n"
                                                        , __FUNCTION__
                                                        , db->name.c_str()
                                                        , name
                                                        , static_cast<int32_t>(StringToControlCode(cjvalue->valuestring))
                                                        );
        sys->setDbVar(curi, name, cjvalue);
        dbs.push_back(std::make_pair(db, flag));
    }
    else if (
            (db->type == Type_Analog) ||
            (db->type == Type_Binary) ||
            (db->type == AnIn16) ||
            (db->type == AnIn32) ||
            (db->type == AnF32)
            )
    {
        if(sys->useReadb[db->type] && (db->readb != NULL))
        {
            FPS_ERROR_PRINT(" ***** %s using readb [%s] for a set to [%s]\n"
                            , __FUNCTION__
                            , db->readb->name.c_str()
                            , db->name.c_str()
                            );
            db = db->readb;
            flag |= PRINT_PARENT;
        }

        sys->setDbVar(curi, name, cjvalue);
        dbs.push_back(std::make_pair(db, flag));
    }
    else
    {
        FPS_ERROR_PRINT( " *************** %s Var [%s] not processed \n",__FUNCTION__, name);  
        free((void*)curi);
        return -1;
    }
    free((void*)curi);
    return dbs.size();   
}

cJSON* sysdbFindAddArray(sysCfg* sysdb, const char* field)
{
    // look for cJSON Object called field
    cJSON* cjf = cJSON_GetObjectItem(sysdb->cj, field);
    if(!cjf)
    {
        cJSON* cja =cJSON_CreateArray();
        cJSON_AddItemToObject(sysdb->cj,field,cja);
        cjf = cJSON_GetObjectItem(sysdb->cj, field);
    }
    return cjf;
}

// this sends out sets for each command received.
void sendCmdSet(sysCfg* sysdb, DbVar*db, cJSON* cj)
{
    const char *uri;
    char turi[1024];

    if (db->uri)
    {
        uri = db->uri;
        if (uri[0] == '/')
        {
            uri = &db->uri[1];
        }
    }
    else
    {
        snprintf(turi, sizeof(turi), "components/%s", sysdb->id );
        uri = (const char *)turi;
    }
    
    char *out = cJSON_PrintUnformatted(cj);
    if (out) 
    {
        char tmp[2048];
        snprintf(tmp, sizeof(tmp), "/%s/%s", uri, db->name.c_str() );

        if(sysdb->p_fims)
        {
            sysdb->p_fims->Send("set", tmp, NULL, out);
        }
        else
        {
            FPS_ERROR_PRINT("%s Error in sysdb->p_fims\n", __FUNCTION__ );
        }    
        free(out);
    }
    else
    {
        FPS_ERROR_PRINT("%s Error in cJSON object\n", __FUNCTION__ );
    }
}
// possibly used in outstation comand handler to publish changes
void sysdbAddtoRecord(sysCfg* sysdb, const char* field, const opendnp3::AnalogOutputInt16& cmd, uint16_t index)
{
    DbVar* db = sysdb->getDbVarId(AnIn16 , index);
    if (db)
    {
        cJSON* cjf = sysdbFindAddArray(sysdb, field);
        cJSON* cjv = cJSON_CreateObject();
        cJSON* cji = cJSON_CreateObject();
        cJSON_AddNumberToObject(cjv,"value", cmd.value);
        cJSON_AddItemToObject(cji,db->name.c_str(),cjv);
        cJSON_AddItemToArray(cjf, cji);
        sysdb->cjloaded++;
        sendCmdSet(sysdb, db, cjv);
        //cJSON_Delete(cjv);
    }
    else
    {
        FPS_ERROR_PRINT("%s unable to find AnIn16 at index %d\n", __FUNCTION__, static_cast<int32_t>(index) );
    }    
}

void sysdbAddtoRecord(sysCfg* sysdb,const char* field, const opendnp3::AnalogOutputInt32& cmd, uint16_t index)
{
    DbVar* db = sysdb->getDbVarId(AnIn32 , index);
    if (db)
    {
        cJSON* cjf = sysdbFindAddArray(sysdb, field);
        cJSON* cjv = cJSON_CreateObject();
        cJSON* cji = cJSON_CreateObject();
        if(db->sign == 1)
        {
            cJSON_AddNumberToObject(cjv,"value", cmd.value);
        }
        else
        {
            uint32_t u32val = static_cast<uint32_t>(cmd.value);
            FPS_ERROR_PRINT("%s usigned AnIn32 val %u at index %d\n", __FUNCTION__, u32val, static_cast<int32_t>(index) );

            cJSON_AddNumberToObject(cjv,"value", static_cast<uint32_t>(cmd.value));            
        }
        
        cJSON_AddItemToObject(cji,db->name.c_str(),cjv);
        cJSON_AddItemToArray(cjf, cji);
        sysdb->cjloaded++;
        sendCmdSet(sysdb, db, cjv);
        //cJSON_Delete(cjv);
    }
    else
    {
        FPS_ERROR_PRINT("%s unable to find AnIn32 at index %d\n", __FUNCTION__, static_cast<int32_t>(index) );
    }

}

void sysdbAddtoRecord(sysCfg* sysdb,const char* field, const opendnp3::AnalogOutputFloat32& cmd, uint16_t index)
{
    DbVar* db = sysdb->getDbVarId(AnF32 , index);
    if (db)
    {
        cJSON* cjf = sysdbFindAddArray(sysdb, field);
        cJSON* cjv = cJSON_CreateObject();
        cJSON* cji = cJSON_CreateObject();
        cJSON_AddNumberToObject(cjv,"value", cmd.value);
        cJSON_AddItemToObject(cji,db->name.c_str(),cjv);
        cJSON_AddItemToArray(cjf, cji);
        sysdb->cjloaded++;
        sendCmdSet(sysdb, db, cjv);
        //cJSON_Delete(cjv);
    }
    else
    {
        FPS_ERROR_PRINT("%s unable to find AnF32 at index %d\n", __FUNCTION__, static_cast<int32_t>(index) );
    }

}

void sysdbAddtoRecord(sysCfg* sysdb, const char* field, const char* cmd, uint16_t index)
{
    DbVar* db = sysdb->getDbVarId(Type_Crob , index);
    if (db)
    {
        cJSON* cjf = sysdbFindAddArray(sysdb, field);
        cJSON* cjv = cJSON_CreateObject();
        cJSON* cji = cJSON_CreateObject();
        cJSON_AddStringToObject(cjv,"value", cmd);
        cJSON_AddItemToObject(cji,db->name.c_str(),cjv);
        cJSON_AddItemToArray(cjf, cji);
        sysdb->cjloaded++;
        sendCmdSet(sysdb, db, cjv);
    }
    else
    {
        FPS_ERROR_PRINT("%s unable to find CROB at index %d\n", __FUNCTION__, static_cast<int32_t>(index) );
    }

}

// TODO allow a setup option in the config file to supply the SOEname
const char* cfgGetSOEName(sysCfg* sysdb, const char* fname)
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
//
// MODBUS does a send on each variable 
// bool process_dnp3_message(int bytes_read, int header_length, datalog* data, system_config* config, server_data* server_map, bool serial, uint8_t * query)
//sprintf(uri, "%s/%s", reg->uri, reg->reg_name);
//                     cJSON_AddBoolToObject(send_body, "value", (value == 0xFF00) ? cJSON_True : cJSON_False);
//                     char* body_msg = cJSON_PrintUnformatted(send_body);
//                     server_map->p_fims->Send("set", uri, NULL, body_msg);

