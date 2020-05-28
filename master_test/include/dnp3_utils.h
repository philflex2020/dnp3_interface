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
#include <vector>

#include "opendnp3/app/AnalogOutput.h"
#include "opendnp3/app/ControlRelayOutputBlock.h"

using namespace opendnp3;

#ifndef FPS_ERROR_PRINT 
#define FPS_ERROR_PRINT printf
#define FPS_DEBUG_PRINT printf
#endif
#define MAX_SETUP_TICKS  50

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

// typedef struct _dnp3_mapping_t {
//     int nb_bits;
//     int start_bits;
//     int nb_input_bits;
//     int start_input_bits;
//     int nb_input_registers;
//     int start_input_registers;
//     int nb_registers;
//     int start_registers;
//     uint8_t *tab_bits;
//     uint8_t *tab_input_bits;
//     uint16_t *tab_input_registers;
//     uint16_t *tab_registers;
// } dnp3_mapping_t;

// enum Type_of_Register {
//     Coil, Discrete_Input, Input_Register, Holding_Register, AnalogIP, AnalogOP, BinaryIP, BinaryOP, Num_Register_Types
// };

// // one of these for each register ??
// typedef struct maps_t{
//     unsigned int    reg_off;
//     unsigned int    num_regs;
//     unsigned int    num_strings;
//     int    shift;
//     double scale;
//     char*  reg_name;
//     char*  uri;
//     bool   sign;
//     bool   floating_pt;
//     bool   is_bool;
//     bool   bit_field;
//     bool   individual_bits;
//     uint64_t data; // used only for individual_bits, read-modify-write construct
//     bool   enum_type;
//     bool   random_enum_type;
//     char ** bit_strings;
//     std::map<int,char*> random_enum;
//     maps_t()
//     {
//         reg_name = uri = NULL;
//         bit_strings = NULL;
//         reg_off = num_regs = num_strings = 0;
//         scale = 0.0;
//         shift = 0;
//         sign = floating_pt = is_bool = false;
//         bit_field = individual_bits = enum_type = random_enum_type = false;
//     }
//     ~maps_t()
//     {
//         if(reg_name != NULL)
//             free (reg_name);
//         if(uri != NULL)
//             free (uri);
//         if(bit_strings != NULL)
//         {
//             for(unsigned int i = 0; i < num_regs * 16; i++)
//                 if(bit_strings[i] != NULL) free(bit_strings[i]);
//             delete [] bit_strings;
//         }
//     }
// } maps;

// //Holds all the information related to registers whose value needs to be read
// typedef struct {
//     Type_of_Register reg_type;
//     unsigned int start_offset;
//     unsigned int num_regs;
//     unsigned int map_size;
//     maps *register_map;
// } datalog;

// // an inividual mapping register
// typedef struct smapr {
//     bool bvalue;
//     float avalue;
//     unsigned int ivalue;
//     int type;
//     smapr(int _type) {
//         avalue = 0.0;
//         bvalue = false;
//         ivalue = 0;
//         type = _type;
//     }
//     ~smapr(){};
    
// } mapr;

// the server data type is given to the dnp3 process
// the dnp3 will load  data items by value type and index and then trigger a send
// typedef struct sdata
// {
//     uri_map uri_to_register;
//     maps** regs_to_map[Num_Register_Types];
//     dnp3_mapping_t* mb_mapping;
//     //void* mb_mapping;
//     fims* p_fims;
//     const char** uris;
//     const char* base_uri;
//     int num_uris;
//     sdata()
//     {
//         memset(regs_to_map, 0, Num_Register_Types * sizeof(maps**));
//         mb_mapping = NULL;
//         p_fims = NULL;
//         uris = NULL;
//         base_uri = NULL;
//         num_uris = 0;
//     }
  
//     int datasendAdd( mapr * mr)
//     {
//         return 0;
//     }

// } server_data;

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

enum Type_of_Var{
    AnIn16,
    AnIn32,
    AnF32,
    Type_Crob,
    Type_Analog,
    Type_Binary,
    NumTypes
};

// used to manage char* compares in maps
struct char_dcmp {
    bool operator () (const char *a,const char *b) const
    {
        return strcmp(a,b)<0;
    }
};
// only allow 1 type for now
enum {
    GroupUndef,
    Group30Var5,
    NumVars
};

int variant_decode(const char* ivar)
{
    if(ivar && (strcmp(ivar,"Group30Var5")== 0))
    {
        return Group30Var5;
    }
    return GroupUndef;
};

// local copy of all inputs and outputs
// Also used with bit fields
// if so the "parent" slot is filled and the index refers to the bit number 
//see https://groups.google.com/forum/#!topic/automatak-dnp3/RvrrCaGM8-8
typedef struct DbVar_t {
    DbVar_t(std::string &_name, int _type, int _offset, const char* iuri, const char*ivariant):name(_name), site(NULL),type(_type), offset(_offset),site_offset(-1) {
        valuedouble = 0.0;
        valueint = 0;
        anInt16 = 0;
        anInt32 = 0;
        anF32 = 0.0;
        crob = 0;
        valflag = 0;
        bit = -1;
        parent = NULL;
        initSet = 0;

        if(iuri)
        {
            uri = strdup(iuri);
        }
        else
        {
            uri = NULL;
        }
        variant = variant_decode(ivariant);
    };

    int addBit(const char*bit)
    {
        dbBits.push_back(std::make_pair(bit,0));
        return (int)dbBits.size();
    }

    int setBit(int idx, int val)
    {
        dbBits[idx].second = val;
        return val;
    }

    int setBit(const char* var, int val)
    {
        for (int i = 0; i < (int)dbBits.size(); i++)
        {
            if(strcmp(dbBits[i].first, var) == 0)
            {
                setBit(i,val);
                return 0;
            }
        }
        return -1;
    }

    int getBit(int idx)
    {
        int val = dbBits[idx].second;
        return val;
    }

    int getBit(const char* var)
    {
        for (int i = 0; i < (int)dbBits.size(); i++)
        {
            if(strcmp(dbBits[i].first, var) == 0)
            {
                return getBit(i);
            }
        }
        return -1;
    }

    // TODO turn these into char* no point the extra strcmp uses more resources
    std::string name;
    const char* site;    // furute use for site.
    const char* uri;
    int valflag;         // set to a1 to enforce the {"name":{"value":val}}  form of output. follows the last set
    int type;
    int variant;         // space to flag different DNP3 variant like Group30var5
    int offset;
    int bit;              // used to indiate which bit in parent
    DbVar_t* parent;      // I'm a bit and this is daddy 
    int site_offset;     // future use for site offset
    // values , one for each type
    double valuedouble;
    int valueint;
    uint16_t anInt16;
    uint32_t anInt32;
    double anF32;
    uint8_t crob;


    // used for bit fields
    std::vector<std::pair<const char*,int>>dbBits;

    uint8_t initSet;
} DbVar;

typedef std::map<std::string, DbVar_t*> dbvar_map;
typedef std::map<int, DbVar_t*>dbix_map;
typedef std::map<const char*,std::vector<DbVar_t*>, char_dcmp>duri_map;

// used in parseBody the int is a print flag to include "value"
typedef std::vector<std::pair<DbVar*,int>>dbs_type; // collect all the parsed vars here

// this is for the bits
typedef std::map<const char*, std::pair<DbVar_t*,int>, char_dcmp>bits_map;


int addVarToCj(cJSON* cj, DbVar*db);
int addVarToCj(cJSON* cj,std::pair<DbVar*,int>dbp);
int addVarToCj(cJSON* cj, const char *dname);
int addVarToCj(cJSON* cj, DbVar* db, int flag);


typedef struct sysCfg_t {

    sysCfg_t() :name(NULL), protocol(NULL), id(NULL), ip_address(NULL), p_fims(NULL)
    {
        cj = NULL;
        cjloaded = 0;
        pub = strdup("MyPubs");  // TODO remove this

    }
    ~sysCfg_t()
    {
        std::cout<<" "<<__FUNCTION__<<" closing\n" ;

        //if(name)free(name);
        if(protocol)free(protocol);
        if(id)free(id);
        if(ip_address)free(ip_address);
        if (pub) free(pub);
        if (name) free(name);

        if (cj) cJSON_Delete(cj);
     // TODO clear out maps
    };

    public:
        DbVar* addDbVar(std::string name, int type, int offset, char* uri, char* variant) 
        {
            DbVar* db = NULL;

            if (dbMap.find(name) == dbMap.end()){
                db = new DbVar(name, type, offset, uri, variant);
                dbMap[name] = db;
                if(dbMapIx[type].find(offset) == dbMapIx[type].end())
                {   
                    dbMapIx[type][offset] = db;
                }
                else
                {
                    FPS_ERROR_PRINT(" %s name [%s] already defined in dbMapIx\n", __FUNCTION__, name.c_str());
                }
            }
            else
            {
                FPS_ERROR_PRINT(" %s name [%s] already defined in dbMap\n", __FUNCTION__, name.c_str());
            }
            return db;
        };

        DbVar* getDbVar(const char* name)
        {
            if (dbMap.find(name) != dbMap.end() )
            {   
                return dbMap[name];
            }
            return NULL;
        };

        int setDb(DbVar* db, double fval)
        {
            if(db != NULL)
            {
                db->initSet = 1;
                switch (db->type) 
                {
                    case Type_Analog:
                    {
                        // also copy valueint or the group30var5 stuff
                        db->valuedouble = fval;
                        db->valueint =  (int)fval;
                        return 1;
                    }    
                    case AnF32:
                    {
                        db->valuedouble = fval;
                        return  1;
                    } 
                    case Type_Binary:
                    {
                        db->valueint = (int)fval;
                        return  1;
                    } 
                    case AnIn32:
                    {
                        db->valueint = (int)fval;
                        db->anInt32 = db->valueint;
                        return  1;
                    }
                   case AnIn16:
                    {
                        db->valueint = (int)fval;
                        db->anInt16 = db->valueint;
                        return  1;
                    }
                    case Type_Crob:
                    {
                    //    db->crob = ControlCodeToType(StringToControlCode(cj->valuestring));
                       return  1;
                    }
                    default:
                        return 0;
                }    
            }
            return 0;
        };

        int setDb(DbVar* db, int ival)
        {
            if(db != NULL)
            {
                db->initSet = 1;
                switch (db->type) 
                {
                    case Type_Analog:
                    {
                        // also copy valueint or the group30var5 stuff
                        db->valuedouble = (double)ival;
                        db->valueint =  ival;
                        return 1;
                    }    
                    case AnF32:
                    {
                        db->valuedouble = (double)ival;
                        return  1;
                    } 
                    case Type_Binary:
                    {
                        db->valueint = ival;
                        return  1;
                    } 
                    case AnIn32:
                    {
                        db->valueint = ival;
                        db->anInt32 = ival;
                        return  1;
                    }
                   case AnIn16:
                    {
                        db->valueint = ival;
                        db->anInt16 = ival;
                        return  1;
                    }
                    case Type_Crob:
                    {
                        db->crob = ival;// ControlCodeToType(StringToControlCode(cj->valuestring));
                        return  1;
                    }
                    default:
                        return 0;
                }
            }
            return 0;
        };

        int setDbVar(const char* name, cJSON* cj)
        {
            DbVar* db = getDbVar(name);
            if(db != NULL)
            {
                switch (db->type) 
                {
                    case Type_Analog:
                    case AnF32:
                    {
                        return setDb(db, cj->valuedouble);
                    }    
                    case Type_Binary:
                    case AnIn32:
                    case AnIn16:
                    {
                        return setDb(db, cj->valueint);
                    } 
                    case Type_Crob:
                    {
                        return setDb(db, ControlCodeToType(StringToControlCode(cj->valuestring)));
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
            if(db != NULL)
            {
                return setDb(db, dval);
            }
            return 0;
        };

        int setDbVar(const char *name, int ival)
        {
            DbVar* db = getDbVar(name);
            if(db != NULL)
            {
                return setDb(db, ival);
            }
            return 0;
        };

        int setDbVarIx(int dbtype, int idx, double fval)
        {
            DbVar* db = NULL;
            if(dbMapIx[dbtype].find(idx) != dbMapIx[dbtype].end())
            {   
                db = dbMapIx[dbtype][idx];
            }
            if (db != NULL)
            {
                return setDb(db, fval);
            }
            return 0;
        };

        int setDbVarIx(int dbtype, int idx, int ival)
        {
            DbVar* db = NULL;
            if(dbMapIx[dbtype].find(idx) != dbMapIx[dbtype].end())
            {   
                db = dbMapIx[dbtype][idx];
            }
            if (db != NULL)
            {
                return setDb(db, ival);
            }
            return 0;
        };

        int getDbIdx(int dbtype, const char* name)
        {
            DbVar* db = getDbVar(name);
            if ((db != NULL) && (db->type == dbtype))
                return db->offset;
            return -1;
        };

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
            FPS_ERROR_PRINT(" %s DbVars===> \n\n", __FUNCTION__);

            dbvar_map::iterator it;
            for (it = dbMap.begin(); it != dbMap.end(); ++it)
            {
                DbVar* db = it->second;
                FPS_ERROR_PRINT(" name :[%s] Type :[%d] offset : [%d] ===> \n"
                            , it->first.c_str()
                            , db->type
                            , db->offset
                            );

            }
            FPS_ERROR_PRINT(" %s DbVars<=== \n\n", __FUNCTION__);
        }

        void addVarsToCj(cJSON* cj)
        {
            dbvar_map::iterator it;
            for (it = dbMap.begin() ; it != dbMap.end();++it)
            {
                DbVar* db = it->second;
                addVarToCj(cj, db);
                FPS_ERROR_PRINT(" name :[%s] Type :[%d] offset : [%d] ===> \n"
                            , it->first.c_str()
                            , db->type
                            , db->offset
                            );  
            }

        }

        void addVarsToVec(std::vector<DbVar*>&dbs)
        {
            dbvar_map::iterator it;
            //int flag = 0;
            for (it = dbMap.begin() ; it != dbMap.end();++it)
            {
                DbVar* db = it->second;
                dbs.push_back(db);
                std::cout << "added to Vector :" <<it->first << " => Type:" << db->type <<" offset :"<<db->offset << '\n';
            }
        }

        void addVarsToVec(dbs_type& dbs)
        {
            dbvar_map::iterator it;
            int flag = 0;
            for (it = dbMap.begin() ; it != dbMap.end();++it)
            {
                DbVar* db = it->second;
                dbs.push_back(std::make_pair(db,flag));
                std::cout << "added to Vector :" <<it->first << " => Type:" << db->type <<" offset :"<<db->offset << '\n';
            }
        }

        void showUris()
        {
            FPS_ERROR_PRINT(" %s uris===> \n\n", __FUNCTION__);

            duri_map::iterator it;
            for (it = uriMap.begin(); it != uriMap.end(); ++it)
            {
                FPS_ERROR_PRINT(" %s uri [%s] num vars %ld\n", __FUNCTION__, it->first, it->second.size());
                for (int i = 0 ; i < (int)it->second.size(); i++ )
                {
                    DbVar* db = it->second[i];
                    FPS_ERROR_PRINT("                 [%s] %d %d\n"
                                    , db->name.c_str() 
                                    , db->type
                                    , db->offset
                                    );
                }
            }
            FPS_ERROR_PRINT(" %s<=== uris \n\n", __FUNCTION__);
        }

        int addBits(DbVar *db, cJSON *bits)
        {
            int asiz = cJSON_GetArraySize(bits);
            for (int i = 0  ; i < asiz; i++ )
            {
                cJSON* cji = cJSON_GetArrayItem(bits, i);
                // just use one copy of valuestring clean up will fix it.
                const char* bs = strdup(cji->valuestring);
                db->addBit(bs);
                bitsMap[bs] == std::make_pair(db, i);
            }
            return asiz;
        }

        // TODO this will not work if we need it we'll  it another way.
        // int subsUris(const char *who)
        // {
        //     FPS_ERROR_PRINT(" %s subscribe to uris===> \n\n", __FUNCTION__);

        //     bool publish_only = false;
        //     duri_map::iterator it;
        //     for (it = uriMap.begin(); it != uriMap.end(); ++it)
        //     {
        //         FPS_ERROR_PRINT(" %s subscribe to uri [%s]\n", __FUNCTION__, it->first);
        //         FPS_ERROR_PRINT(" %s subscribe to id [%s]\n", __FUNCTION__, id);

        //         char replyto[1024];
        //         const char* subs[1];
        //         //
        //         if (it->first[0] == '/') 
        //         {
        //             snprintf(replyto, sizeof(replyto),"/interfaces/%s/%s/reply%s", id, who, it->first);
        //         } 
        //         else
        //         {
        //             snprintf(replyto, sizeof(replyto),"/interfaces/%s/%s/reply/%s", id, who, it->first);
        //         }
        //         subs[0] = replyto;

        //         FPS_ERROR_PRINT(" %s subscribe to replyto [%s]\n", __FUNCTION__, replyto);
        //         if(p_fims->Subscribe((const char**)subs, 1, (bool *)&publish_only) == false)
        //         {
        //             FPS_ERROR_PRINT("Subscription to [%s] failed.\n", replyto);
        //         }
        //         else
        //         {
        //             FPS_ERROR_PRINT("Subscription to [%s] passed.\n", replyto);
        //         }
        //     }
        //     FPS_ERROR_PRINT(" %s subscribe to uris<== DONE \n\n", __FUNCTION__);
        //     return 0;
        // }

        bool checkUris(const char *who)
        {
            int outs = strcmp(who, "outstation");
            duri_map::iterator it;
            for (it = uriMap.begin(); it != uriMap.end(); ++it)
            {
                for (int i = 0; i < (int)it->second.size(); i++ )
                {
                    DbVar* db = it->second[i];
                    if (db->initSet == 0)
                    {
                        if(outs==0)
                        {
                            if ((db->type == Type_Analog) || (db->type == Type_Analog))
                            {
                                return false;
                            }
                        }
                        else // all others are on master
                        {
                            if (
                                (db->type == AnIn16) 
                                || (db->type == AnIn32)
                                || (db->type == AnF32 )
                                || (db->type == Type_Crob)
                                )
                            { 
                                return false;
                            }
                        }
                    }
                }
            }
            return true;
        };

        // TODO only get the ones for vars applied to this application (outstation or master)
        int getUris(const char *who)
        {
            FPS_ERROR_PRINT(" %s uris===>%s<=== \n\n", __FUNCTION__, who);

            duri_map::iterator it;
            for (it = uriMap.begin(); it != uriMap.end(); ++it)
            {
                char replyto[1024];
                char getUri[1024];
                if (it->first[0] == '/') 
                {
                    snprintf(replyto, sizeof(replyto),"/interfaces/%s/%s/reply%s", who, id, it->first);
                    snprintf(getUri,sizeof(getUri),"/queryx%s", it->first);
                } 
                else
                {
                    snprintf(replyto, sizeof(replyto),"/interfaces/%s/%s/reply/%s", who, id, it->first);
                    snprintf(getUri,sizeof(getUri),"/queryx/%s", it->first);
                }

                FPS_ERROR_PRINT(" uri : [%s] replyto: [%s]\n"
                    , getUri
                    , replyto
                    );
        
                p_fims->Send("get", getUri, replyto, NULL);
            }
            FPS_ERROR_PRINT(" %s<=== get uris DONE\n\n", __FUNCTION__);
            return 0;
        }

        void addUri(const char *uri, DbVar*db)
        {
            const char *mapUri;
            // this is a pointer to the uri 
            // if there is not one in the map then create a new one and then add it
            //duri_map::iterator it_uris;
            if(uriMap.find(uri) == uriMap.end())
            {
                mapUri = strdup(uri);
                uri = mapUri;
            }
            uriMap[uri].push_back(db);
        }

        void addUri(cJSON* uri, DbVar*db)
        {
            if(uri && (uri->valuestring))
            {
                return addUri(uri->valuestring, db);
            }
        }

        void addDefUri(DbVar*db)
        {
            return addUri(id, db);
        }
        
        char* name;
        char* protocol;
        char* id;
        char* ip_address;
        char* pub;
        int version;
        int port;
        int local_address;
        int remote_address;
        int frequency;

        // new way of doing this
        dbvar_map dbMap;
        dbix_map dbMapIx[Type_of_Var::NumTypes];
        duri_map uriMap;
        bits_map bitsMap;

        int numObjs[Type_of_Var::NumTypes];

        fims* p_fims;
        cJSON* cj;  
        int cjloaded;

} sysCfg;

DbVar* getDbVar(sysCfg *cfgdb, const char *name);

//bool process_dnp3_message(int bytes_read, int header_length, datalog* data, system_config* config, server_data* server_map, bool serial, uint8_t * query);
//bool update_register_value(dnp3_mapping_t* map, bool* reg_type, maps** settings, cJSON* value);
void emit_event(fims* pFims, const char* source, const char* message, int severity);
cJSON* get_config_json(int argc, char* argv[]);
//server_data* create_register_map(cJSON* registers, datalog* data);
//int parse_system(cJSON *system, system_config *config);

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
int addVarToCj(sysCfg* sys, cJSON* cj, const char* dname);

cJSON* parseBody( dbs_type&dbs, sysCfg*sys, fims_message*msg, const char* who);
int addValueToVec(dbs_type&dbs, sysCfg*sys, /*CommandSet& commands,*/ const char* valuestring, cJSON *cjvalue,int flag);
// int addValueToDb(sysCfg*sys, const char* name , cJSON *cjvalue, int flag);
bool checkWho(sysCfg*sys, const char *name, const char *who);
bool checkWho(sysCfg*sys, DbVar *db, const char *who);
#endif
