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
void pubWithTimeStamp(cJSON *cj, sysCfg* sys, const char* ev)
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
//    },


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
    //if(ret) ret = getCJstr(cj,"name",sys->name);

    // config file has "objects" with children groups "binary" and "analog"
    return ret;
}

bool parse_variables(cJSON* object, sysCfg* sys)
{
    cJSON *id, *offset;

    // config file has "objects" with children groups "binary" and "analog"
    cJSON *JSON_objects = cJSON_GetObjectItem(object, "objects");
    if (JSON_objects == NULL)
    {
        FPS_ERROR_PRINT("objects object is missing from file! \n");
        return false;
    }

    cJSON *JSON_binary = cJSON_GetObjectItem(JSON_objects, "binary");
    if (JSON_binary == NULL)
    {
        FPS_ERROR_PRINT("binary object is missing from file! \n");
        return false;
    }
    sys->numBinaries = cJSON_GetArraySize(JSON_binary);
    for(uint i = 0; i < sys->numBinaries; i++)
    {
        cJSON* binary_object = cJSON_GetArrayItem(JSON_binary, i);
        if(binary_object == NULL)
        {
            FPS_ERROR_PRINT("Invalid or NULL binary #%d\n", i);
            return false;
        }
        id = cJSON_GetObjectItem(binary_object, "id");
        offset = cJSON_GetObjectItem(binary_object, "offset");
        if (id == NULL || offset == NULL || id->valuestring == NULL)
        {
            FPS_ERROR_PRINT("NULL variables or component_id for %d\n", i);
            return false;
        }
        sys->binaryNames[offset->valueint] = strdup(id->valuestring);   // assume this takes a copy
        sys->binaryIdx[strdup(id->valuestring)] = offset->valueint;   
        std::cout << " config adding Binary name ["<<id->valuestring<<"] id [" << offset->valueint << "]\n";
    }

    cJSON *JSON_analog = cJSON_GetObjectItem(JSON_objects, "analog");
    if (JSON_analog == NULL)
    {
        FPS_ERROR_PRINT("analog object is missing from file! \n");
        return false;
    }
    sys->numAnalogs = cJSON_GetArraySize(JSON_analog);
    for(uint i = 0; i < sys->numAnalogs; i++)
    {
        cJSON* analog_object = cJSON_GetArrayItem(JSON_analog, i);
        if(analog_object == NULL)
        {
            FPS_ERROR_PRINT("Invalid or NULL analog #%d\n", i);
            return false;
        }
        id = cJSON_GetObjectItem(analog_object, "id");
        offset = cJSON_GetObjectItem(analog_object, "offset");
        if (id == NULL || offset == NULL || id->valuestring == NULL)
        {
            FPS_ERROR_PRINT("NULL variables or component_id for %d\n", i);
            return false;
        }
        sys->analogNames[offset->valueint] = strdup(id->valuestring);   // assume this tkes a copy
        sys->analogIdx[strdup(id->valuestring)] = offset->valueint;   
        std::cout << " config adding Analog name ["<<id->valuestring<<"] id [" << offset->valueint << "]\n";
    }
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

}
void cfgdbAddtoRecord(sysCfg* cfgdb,const char* field, const opendnp3::AnalogOutputInt16& cmd, uint16_t index)
{
    cJSON* cjf = cfgdbFindAddArray(cfgdb, field);
    cJSON*cji = cJSON_CreateObject();
    // todo resolve name
    cJSON_AddNumberToObject(cji,"offset",index);
    //const char* indexName = cfgindextoName(AOP16,index);
    //cJSON_AddStringToObject(cji,"offset",indexName);
    cJSON_AddNumberToObject(cji,"value", cmd.value);
    cJSON_AddItemToArray(cjf,cji);

}
void cfgdbAddtoRecord(sysCfg* cfgdb,const char* field, const opendnp3::AnalogOutputInt32& cmd, uint16_t index)
{
    cJSON* cjf = cfgdbFindAddArray(cfgdb, field);
    cJSON*cji = cJSON_CreateObject();
    // todo resolve name
    cJSON_AddNumberToObject(cji,"offset",index);
    //const char* indexName = cfgindextoName(AOP32,index);
    //cJSON_AddStringToObject(cji,"offset",indexName);
    cJSON_AddNumberToObject(cji,"value", cmd.value);
    cJSON_AddItemToArray(cjf,cji);

}
void cfgdbAddtoRecord(sysCfg* cfgdb,const char* field, const opendnp3::AnalogOutputFloat32& cmd, uint16_t index)
{
    cJSON* cjf = cfgdbFindAddArray(cfgdb, field);
    cJSON*cji = cJSON_CreateObject();
    // todo resolve name
    cJSON_AddNumberToObject(cji,"offset",index);
    //const char* indexName = cfgindextoName(Float32,index);
    //cJSON_AddStringToObject(cji,"offset",indexName);
    cJSON_AddNumberToObject(cji,"value", cmd.value);
    cJSON_AddItemToArray(cjf,cji);

}
void cfgdbAddtoRecord(sysCfg* cfgdb,const char* field, const ControlRelayOutputBlock& cmd, uint16_t index)
{
    cJSON* cjf = cfgdbFindAddArray(cfgdb, field);
    cJSON*cji = cJSON_CreateObject();
    // todo resolve name
    cJSON_AddNumberToObject(cji,"offset",index);
    //const char* indexName = cfgindextoName(CROB,index);
    //cJSON_AddStringToObject(cji,"offset",indexName);
    cJSON_AddNumberToObject(cji,"value", cmd.value);
    cJSON_AddItemToArray(cjf,cji);

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
