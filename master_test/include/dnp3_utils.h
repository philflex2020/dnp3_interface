#ifndef DNP3_UTILS_H
#define DNP3_UTILS_H
/*
 * mapping shared library
 *      pwilshire      5/5/2020
 */

#include <cjson/cJSON.h>
#include <fims/libfims.h>

#include <map>
#include <cstring>
#include <string>
#include <iostream>

#include "opendnp3/app/AnalogOutput.h"
#include "opendnp3/app/ControlRelayOutputBlock.h"

using namespace opendnp3;

#ifndef FPS_ERROR_PRINT 
#define FPS_ERROR_PRINT printf
#define FPS_DEBUG_PRINT printf
#endif


struct char_cmp {
    bool operator () (const char *a,const char *b) const
    {
        return strcmp(a,b)<0;
    }
};
typedef struct maps_t maps;
typedef std::map<const char*, std::pair<bool*, maps**>, char_cmp> body_map;
typedef std::map<const char*, body_map*, char_cmp> uri_map;

//typedef struct _modbus modbus_t;

typedef struct _dnp3_mapping_t {
    int nb_bits;
    int start_bits;
    int nb_input_bits;
    int start_input_bits;
    int nb_input_registers;
    int start_input_registers;
    int nb_registers;
    int start_registers;
    uint8_t *tab_bits;
    uint8_t *tab_input_bits;
    uint16_t *tab_input_registers;
    uint16_t *tab_registers;
} dnp3_mapping_t;

enum Type_of_Register {
    Coil, Discrete_Input, Input_Register, Holding_Register, AnalogIP, AnalogOP, BinaryIP, BinaryOP, Num_Register_Types
};

typedef struct {
    char* name;
    char* ip_address;
    char* serial_device;
    char* eth_dev;
    bool byte_swap;
    bool off_by_one;
    char parity;
    int frequency;
    int port;
    int baud;
    int data;
    int stop;
    int device_id;
    int reg_cnt;
    //modbus_t *mb;
    void *mb;
} system_config;

// one of these for each register ??
typedef struct maps_t{
    unsigned int    reg_off;
    unsigned int    num_regs;
    unsigned int    num_strings;
    int    shift;
    double scale;
    char*  reg_name;
    char*  uri;
    bool   sign;
    bool   floating_pt;
    bool   is_bool;
    bool   bit_field;
    bool   individual_bits;
    uint64_t data; // used only for individual_bits, read-modify-write construct
    bool   enum_type;
    bool   random_enum_type;
    char ** bit_strings;
    std::map<int,char*> random_enum;
    maps_t()
    {
        reg_name = uri = NULL;
        bit_strings = NULL;
        reg_off = num_regs = num_strings = 0;
        scale = 0.0;
        shift = 0;
        sign = floating_pt = is_bool = false;
        bit_field = individual_bits = enum_type = random_enum_type = false;
    }
    ~maps_t()
    {
        if(reg_name != NULL)
            free (reg_name);
        if(uri != NULL)
            free (uri);
        if(bit_strings != NULL)
        {
            for(unsigned int i = 0; i < num_regs * 16; i++)
                if(bit_strings[i] != NULL) free(bit_strings[i]);
            delete [] bit_strings;
        }
    }
} maps;

//Holds all the information related to registers whose value needs to be read
typedef struct {
    Type_of_Register reg_type;
    unsigned int start_offset;
    unsigned int num_regs;
    unsigned int map_size;
    maps *register_map;
} datalog;

// an inividual mapping register
typedef struct smapr {
    bool bvalue;
    float avalue;
    unsigned int ivalue;
    int type;
    smapr(int _type) {
        avalue = 0.0;
        bvalue = false;
        ivalue = 0;
        type = _type;
    }
    ~smapr(){};
    
} mapr;
// the server data type is given to the dnp3 process
// the dnp3 will load  data items by value type and index and then trigger a send
typedef struct sdata
{
    uri_map uri_to_register;
    maps** regs_to_map[Num_Register_Types];
    dnp3_mapping_t* mb_mapping;
    //void* mb_mapping;
    fims* p_fims;
    const char** uris;
    const char* base_uri;
    int num_uris;
    sdata()
    {
        memset(regs_to_map, 0, Num_Register_Types * sizeof(maps**));
        mb_mapping = NULL;
        p_fims = NULL;
        uris = NULL;
        base_uri = NULL;
        num_uris = 0;
    }
    int setValue(int idx, float value, int send) 
    {
     //   maps* mp = regs_to_map[AnalogIP];
     //   if (idx < mp->num_regs)
     //   {
     //       mapr* mr = mp->regs[idx];
     //       mapr->avalue = value;
     //       if(send)datasendAdd(mr);
     //   }
        return 0;
    }

    int setValue(int idx, bool value, int send) 
    {
     //   maps* mp = regs_to_map[BinaryIP];
     //   if (idx < mp->num_regs)
     //   {
     //       mapr* mr = mp->regs[idx];
     //       mapr->bvalue = value;
     //       if(send)datasendAdd(mr);
     //   }
        return 0;
    }

    int datasendAdd( mapr * mr)
    {
        return 0;
    }

} server_data;

// "system": {
//        "protocol": "DNP3",
//        "version": 1,
//        "id": "dnp3_outstation",
//        "ip_address": "192.168.1.50",
//        "port": 502,
//        "local_address": 1,
//		"remote_address": 0
//    },
//TODO use real types
// test code for dnp3_utils

#define AnIn16 1
#define AnIn32 2
#define AnF32 3
#define Crob 4
#define Type_Analog 5
#define Type_Binary 6

//see https://groups.google.com/forum/#!topic/automatak-dnp3/RvrrCaGM8-8
typedef struct DbVar_t {
    DbVar_t(std::string _name, int _type, int _offset):name(_name), site(""),type(_type), offset(_offset),site_offset(-1){
        valuedouble = 0.0;
        valueint = 0;
        anInt16 = 0;
        anInt32 = 0;
        anF32 = 0.0;
        crob = 0;
    };
    std::string name;
    std::string site;
    int type;
    int offset;
    int site_offset;
    // todo outputs
    double valuedouble;
    int valueint;
    uint16_t anInt16;
    uint32_t anInt32;
    double anF32;
    uint8_t crob;
    

} DbVar;

typedef struct sysCfg_t {

    sysCfg_t() :name(NULL), protocol(NULL), id(NULL), ip_address(NULL), p_fims(NULL)
    {
        cj = NULL;
        numBinaries  = 0;
        numAnalogs  = 0;
        cjloaded = 0;
        pub = strdup("MyPubs");

    }
    ~sysCfg_t()
    {
        std::cout<<" "<<__FUNCTION__<<" closing\n" ;

        if(name)free(name);
        if(protocol)free(protocol);
        if(id)free(id);
        if(ip_address)free(ip_address);
        if (pub) free(pub);
        clearBinaries();
        clearAnalogs();
        if (cj) cJSON_Delete(cj);
        //todo fims
        }

    public:
        char* getAnalog(int idx) 
        {
            if(analogNames.find(idx) != analogNames.end())
            {
                return analogNames[idx];
            }
            else
            {
                return (char *) "Unknown";
            }
        }
        char* getBinary(int idx) 
        {
            if(binaryNames.find(idx) != binaryNames.end())
            {
                return binaryNames[idx];
            }
            else
            {
                return (char *) "Unknown";
            }
        }

        int  getAnalogIdx(char *name) 
        {
            int idx = -1;
            std::map<char*,int>::iterator it_ids;
            for (it_ids = analogIdx.begin() ; it_ids != analogIdx.end();++it_ids)
            {
                std::cout << __FUNCTION__ << it_ids->first << " => " << it_ids->second << '\n';
                if(strcmp(it_ids->first, name)== 0 )
                {
                   idx = it_ids->second;
                   break;
                }
            }
            //if(analogIdx.find(name) != analogIdx.end())
            //{
            //    idx = analogIdx[name];
            //}
            //else
            //{
            //    idx =  -1;
            //}
            std::cout << " Seeking Analog ["<< name <<"] found [" << idx <<"]\n";
            return idx;

        }

        void addDbVar(std::string name, int type, int offset) 
        {
            if (dbMap.find(name) == dbMap.end()){
                DbVar* db = new DbVar(name, type, offset);
                dbMap[name] = db;
            }
            else
            {
                std::cout << __FUNCTION__<< " name [" << name <<"] allready defined  in dbMap\n";                
            }
            
        }

        DbVar* getDbVar(const char *name)
        {
            if (dbMap.find(name) != dbMap.end() )
            {   
                return dbMap[name];
            }
            return NULL;
        };

        int  getBinaryIdx(char *name) 
        {
            int idx = -1;
            std::map<char*,int>::iterator it_ids;
            for (it_ids = binaryIdx.begin() ; it_ids != binaryIdx.end();++it_ids)
            {
                std::cout << __FUNCTION__<< it_ids->first << " => " << it_ids->second << '\n';
                if(strcmp(it_ids->first, name)== 0 )
                {
                   idx = it_ids->second;
                   break;
                }
            }

            std::cout << " Seeking Binary ["<< name <<"] found [" << idx << "]\n";
            return idx;
        }

        void showBinaries()
        {
            std::cout << " Binary maps\n" ;
            std::map<int , char*>::iterator it_names;
            for (it_names = binaryNames.begin() ; it_names != binaryNames.end();++it_names)
            {
                std::cout << it_names->first << " => " << it_names->second << '\n';
            }
            std::map<char*,int>::iterator it_ids;
            for (it_ids = binaryIdx.begin() ; it_ids != binaryIdx.end();++it_ids)
            {
                std::cout << it_ids->first << " => " << it_ids->second << '\n';
            }

        }
        
        void showAnalogs()
        {
            std::cout << " Clear Analog maps\n" ;
            std::map<int, char *>::iterator it_names;
            for (it_names = analogNames.begin() ; it_names != analogNames.end();++it_names)
            {
                std::cout << it_names->first << " => " << it_names->second << '\n';
            }
            std::map<char*,int>::iterator it_ids;
            for (it_ids = analogIdx.begin() ; it_ids != analogIdx.end();++it_ids)
            {
                std::cout << it_ids->first << " => " << it_ids->second << '\n';
            }

        }
        void showDbMap()
        {
            std::cout << " Clear Analog maps\n" ;
            std::map<std::string, DbVar *>::iterator it_vars;
            for (it_vars = dbMap.begin() ; it_vars != dbMap.end();++it_vars)
            {
                DbVar* db = it_vars->second;
                std::cout << it_vars->first << " => Type:" << db->type <<" offset :"<<db->offset << '\n';
            }
        }

        void clearBinaries()
        {
            std::cout << " Binary maps\n" ;
            std::map<int , char*>::iterator it_names;
            for (it_names = binaryNames.begin() ; it_names != binaryNames.end();++it_names)
            {
                std::cout << it_names->first << " => " << it_names->second << '\n';
                free(it_names->second);
            }
            binaryNames.clear();

            std::map<char*,int>::iterator it_ids;
            for (it_ids = binaryIdx.begin() ; it_ids != binaryIdx.end();++it_ids)
            {
                std::cout << it_ids->first << " => " << it_ids->second << '\n';
                free(it_ids->first);
            }
            binaryIdx.clear();

        }

        void clearAnalogs()
        {
            std::cout << " Analog maps\n" ;
            std::map<int, char *>::iterator it_names;
            for (it_names = analogNames.begin() ; it_names != analogNames.end();++it_names)
            {
                std::cout << it_names->first << " => " << it_names->second << '\n';
                free(it_names->second); 
            }
            analogNames.clear();

            std::map<char*,int>::iterator it_ids;
            for (it_ids = analogIdx.begin() ; it_ids != analogIdx.end();++it_ids)
            {
                std::cout << it_ids->first << " => " << it_ids->second << '\n';
                free(it_ids->first);
            }
            analogIdx.clear();

        }

        char* name;
        char* protocol;
        int version;
        char* id;
        char* ip_address;
        char* pub;
        int port;
        int local_address;
        int remote_address;
        uint numAnalogs;
        uint numBinaries;
        // really need an array of these 
        std::map<int,char*>binaryNames;
        std::map<int,char*>analogNames;
        std::map<char*,int>binaryIdx;
        std::map<char*,int>analogIdx;
        std::map<std::string , DbVar *> dbMap;

        fims* p_fims;
        cJSON* cj;
        int cjloaded;

} sysCfg;

DbVar* getDbVar(sysCfg *cfgdb, const char *name);

bool process_dnp3_message(int bytes_read, int header_length, datalog* data, system_config* config, server_data* server_map, bool serial, uint8_t * query);
bool update_register_value(dnp3_mapping_t* map, bool* reg_type, maps** settings, cJSON* value);
void emit_event(fims* pFims, const char* source, const char* message, int severity);
cJSON* get_config_json(int argc, char* argv[]);
server_data* create_register_map(cJSON* registers, datalog* data);
int parse_system(cJSON *system, system_config *config);

// new mapping
bool parse_system(cJSON* object, sysCfg* sys);
bool parse_variables(cJSON* object, sysCfg* sys);
cJSON *parseJSONConfig(char *file_path);
void addCjTimestamp(cJSON *cj, const char * ts);
void pubWithTimeStamp(cJSON *cj, sysCfg* sys, const char* ev);
void pubWithTimeStamp2(cJSON *cj, sysCfg* sys, const char* ev);

void cfgdbAddtoRecord(sysCfg* cfgdb,const char* field, const AnalogOutputInt16& cmd, uint16_t index);
void cfgdbAddtoRecord(sysCfg* cfgdb,const char* field, const AnalogOutputInt32& cmd, uint16_t index);
void cfgdbAddtoRecord(sysCfg* cfgdb,const char* field, const AnalogOutputFloat32& cmd, uint16_t index);
void cfgdbAddtoRecord(sysCfg* cfgdb,const char* field, const char* cmd, uint16_t index);
const char* cfgGetSOEName(sysCfg* cfgdb, const char* fname);

#endif
