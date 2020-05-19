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

const ControlCode StringToControlCode(const char* codeWord);

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
    // int setValue(int idx, float value, int send) 
    // {
    //  //   maps* mp = regs_to_map[AnalogIP];
    //  //   if (idx < mp->num_regs)
    //  //   {
    //  //       mapr* mr = mp->regs[idx];
    //  //       mapr->avalue = value;
    //  //       if(send)datasendAdd(mr);
    //  //   }
    //     return 0;
    // }

    // int setValue(int idx, bool value, int send) 
    // {
    //  //   maps* mp = regs_to_map[BinaryIP];
    //  //   if (idx < mp->num_regs)
    //  //   {
    //  //       mapr* mr = mp->regs[idx];
    //  //       mapr->bvalue = value;
    //  //       if(send)datasendAdd(mr);
    //  //   }
    //     return 0;
    // }

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
enum {
    AnIn16,
    AnIn32,
    AnF32,
    Type_Crob,
    Type_Analog,
    Type_Binary,
    NumTypes
};

// local copy of all inputs and outputs
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
    // outputs
    double valuedouble;
    int valueint;
    uint16_t anInt16;
    uint32_t anInt32;
    double anF32;
    uint8_t crob;
    

} DbVar;

int addVarToCj(cJSON* cj, DbVar*db);
typedef struct sysCfg_t {

    sysCfg_t() :name(NULL), protocol(NULL), id(NULL), ip_address(NULL), p_fims(NULL)
    {
        cj = NULL;
        //numBinaries  = 0;
        //numAnalogs  = 0;
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
        //clearBinaries();
        //clearAnalogs();
        if (cj) cJSON_Delete(cj);
        //todo fims
    }

    public:
        
        void addDbVar(std::string name, int type, int offset) 
        {
            DbVar* db = NULL;

            if (dbMap.find(name) == dbMap.end()){
                db = new DbVar(name, type, offset);
                dbMap[name] = db;
                if(dbMapIx[type].find(offset) == dbMapIx[type].end())
                {   
                    dbMapIx[type][offset] = db;
                }
                else
                {
                    std::cout << __FUNCTION__<< " name [" << name <<"] already defined  in dbMapIx\n";                
                }
            }
            else
            {
                std::cout << __FUNCTION__<< " name [" << name <<"] already defined  in dbMap\n";                
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

        int setDbVar(const char* name, cJSON* cj)
        {
            DbVar* db = getDbVar(name);
            if(db != NULL)
            {
                switch (db->type) 
                {
                    case Type_Analog:
                    {
                    db->valuedouble = cj->valuedouble;
                    return 1;
                    }    
                    case AnF32:
                    {
                        db->valuedouble = cj->valuedouble;
                        return  1;
                    } 
                    case Type_Binary:
                    {
                        db->valueint = cj->valueint;
                        return  1;
                    } 
                    case AnIn32:
                    {
                        db->valueint = cj->valueint;
                        db->anInt32 = cj->valueint;
                        return  1;
                    }
                   case AnIn16:
                    {
                        db->valueint = cj->valueint;
                        db->anInt16 = cj->valueint;
                        return  1;
                    }
                    case Type_Crob:
                    {
                        //db->crob = ControlCodeToType(StringToControlCode(cj->valuestring));
                        db->crob = StringToControlCode(cj->valuestring);
                        db->valueint = cj->valueint;
                        db->anInt16 = cj->valueint;
                        return  1;
                    }
                    default:
                        return 0;
                }
            }
            return 0;
        };

        int setDbVar(const char *name, double dval)
        {
            DbVar* db = getDbVar(name);
            if ((db != NULL) && (db->type == Type_Analog))
            {
                db->valuedouble = dval;
                return 1;
            } 
            if ((db != NULL) && (db->type == AnF32))
            {
                db->valuedouble = dval;
                return  1;
            } 
            return 0;
        };

        int setDbVar(const char *name, int ival)
        {
            DbVar* db = getDbVar(name);
            if ((db != NULL) && (db->type == Type_Binary))
            {
                db->valueint = ival;
                return  1;
            } 
            if ((db != NULL) && (db->type == AnIn32))
            {
                db->valueint = ival;
                db->anInt32 = ival;
                return  1;
            } 
            if ((db != NULL) && (db->type == AnIn16))
            {
                db->valueint = ival;
                db->anInt16 = ival;
               return  1;
            } 
            return 0;
        };

        void setDbVarIx(int dbtype, int idx, double fval)
        {
            DbVar* db = NULL;
            if(dbMapIx[dbtype].find(idx) != dbMapIx[dbtype].end())
            {   
                db = dbMapIx[dbtype][idx];
            }
            if ((db != NULL) && (db->type == AnF32))
            {
                db->valuedouble = fval;
            } 
        };

        void setDbVarIx(int dbtype, int idx, int ival)
        {
            DbVar* db = NULL;
            
            if(dbMapIx[dbtype].find(idx) != dbMapIx[dbtype].end())
            {   
                db = dbMapIx[dbtype][idx];
            }
            if ((db != NULL) && (db->type == Type_Binary))
            {
                db->valueint = ival;
            } 
            if ((db != NULL) && (db->type == AnIn32))
            {
                db->valueint = ival;
                db->anInt32 = ival;

            } 
            if ((db != NULL) && (db->type == AnIn16))
            {
                db->valueint = ival;
                db->anInt16 = ival;
            } 
        };

        int getDbIdx(int dbtype, const char* name)
        {
            DbVar* db = getDbVar(name);
            if ((db != NULL) && (db->type == dbtype))
                return db->offset;
            return -1;
        }

        DbVar* getDbVarId(int dbtype, int idx)
        {
            if(dbMapIx[dbtype].find(idx) != dbMapIx[dbtype].end())
            {   
                return dbMapIx[dbtype][idx];
            }
            return NULL;
        };
        
        void showDbMap()
        {
            std::cout << " show DbVars\n" ;
            std::map<std::string, DbVar *>::iterator it_vars;
            for (it_vars = dbMap.begin() ; it_vars != dbMap.end();++it_vars)
            {
                DbVar* db = it_vars->second;
                std::cout << it_vars->first << " => Type:" << db->type <<" offset :"<<db->offset << '\n';
            }
        }

        void addVarsToCj(cJSON* cj)
        {
            std::map<std::string, DbVar *>::iterator it_vars;
            for (it_vars = dbMap.begin() ; it_vars != dbMap.end();++it_vars)
            {
                DbVar* db = it_vars->second;
                addVarToCj(cj, db);
                std::cout << it_vars->first << " => Type:" << db->type <<" offset :"<<db->offset << '\n';
            }

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

        // new way of doing this
        std::map<std::string, DbVar*> dbMap;
        std::map<int, DbVar *> dbMapIx[NumTypes];
        int numObjs[NumTypes];

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
const char *iotypToStr (int t);
int iotypToId (const char* t);
int addVarToCj(cJSON* cj, DbVar*db);


#endif
