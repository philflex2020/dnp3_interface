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
#define DNP3_MASTER 0
#define DNP3_OUTSTATION 1


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
//typedef std::vector<std::pair<const char*,int>> bits_map;

int variation_decode(const char* ivar);
const char *iotypToStr (int t);
int iotypToId (const char* t);
int countUris(const char* uri);

enum Type_of_Var{
    AnIn16,
    AnIn32,
    AnF32,
    Type_Crob,     // write master
    Type_Analog,  // write outstation
    Type_Binary,     //write outstation
    Type_AnalogOS,  // auto write outstation
    Type_BinaryOS,  //write outstation
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
    Group30Var2,
    Group30Var5,
    Group32Var7,
    NumVars
};

// flag options for reporting
#define PRINT_VALUE      1 << 0
#define PRINT_PARENT     1 << 1

// local copy of all inputs and outputs
// Also used with bit fields
// if so the "parent" slot is filled and the index refers to the bit number 
//see https://groups.google.com/forum/#!topic/automatak-dnp3/RvrrCaGM8-8
typedef struct DbVar_t {
    DbVar_t(std::string &_name, int _type, int _offset, const char* iuri, const char*ivariation):name(_name), site(NULL),type(_type), offset(_offset),site_offset(-1) {
        valuedouble = 0.0;
        //valueint = 0;
        //xnInt16 = 0;
        //anInt32 = 0;
        //xnF32 = 0.0;
        crob = 0;
        bit = -1;
        parent = NULL;
        initSet = 0;
        readb = NULL;
        linkb = NULL;  // used to link outstation responses to master vars
        linkback = NULL;
        clazz = 0;
        sign = false;
        scale = 0;
     

        if(iuri)
        {
            uri = strdup(iuri);
        }
        else
        {
            uri = NULL;
        }
        variation = variation_decode(ivariation);
    };

    ~DbVar_t()
    {
        if(uri)free((void *)uri);
        if(linkback) free((void *)linkback);
    }

    void setEvar(const char* evar)
    {
        evariation = variation_decode(evar);
    }

    void setClazz(int iclazz)
    {
        clazz = iclazz;
    }
    
    int addBit(const char*bit)
    {
        dbBits.push_back(std::make_pair(bit,0));
        return static_cast<int32_t>(dbBits.size());
    }

    int setBit(int idx, int val)
    {
        dbBits[idx].second = val;
        return val;
    }

    int setBit(const char* var, int val)
    {
        for (int i = 0; i < static_cast<int32_t>(dbBits.size()); i++)
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
        for (int i = 0; i < static_cast<int32_t>(dbBits.size()); i++)
        {
            if(strcmp(dbBits[i].first, var) == 0)
            {
                return getBit(i);
            }
        }
        return -1;
    }

    // TODO turn these into char* .... no point the extra strcmp uses more resources
    std::string name;
    const char* site;    // furute use for site.
    const char* uri;
    int type;
    int variation;         // space to flag different DNP3 variation like Group30var5
    int evariation;         // space to flag different DNP3 variation like Group30var5
    int offset;
    int bit;              // used to indiate which bit in parent
    DbVar_t* parent;      // I'm a bit and this is daddy 
    int site_offset;     // future use for site offset
    // values , one for each type
    double valuedouble;
    //int valueint;
    //int16_t xnInt16;
    //int32_t anInt32;
    //double xnF32;
    uint8_t crob;
    int idx;      // type index
    int clazz;    // class

    // used for bit fields
    std::vector<std::pair<const char*,int>>/*bits_map*/dbBits;

    uint8_t initSet;
    DbVar_t* readb;      // we have a readb linked to this  
    DbVar_t* linkb;
    const char* linkback;
    bool sign;
    int scale;

} DbVar;


typedef std::map<std::string, DbVar_t*> dbvar_map;
typedef std::map<int, DbVar_t*>dbix_map;
typedef std::map<const char*,std::vector<DbVar_t*>, char_dcmp>duri_map;
typedef std::vector<DbVar_t*> dbvec;

// new thinking 
// we'll have a map of uris each with a pointer to a map of variables
// we'll have one of these for pubs , gets , and sets
// how do we group them together
// each varuable can have a uri or a default one establised for the component
// "compname" :{
//    "uri":" <some uri>",
//    "varibles":[
      // "varname":{.....},
      // "varname":{.....}
//    ]
// }
// 
// not going to do it this way
// the uri is fixed for a var
// we can add _get or _set to the uri for special uses.
//this structure is still good. the uri points to a map
// just like john did
// but it can be a real map we dont need to share them
// so  when we add a var we include the uri
//typedef std::map<std::string, dbvar_map> dburi_map;


// used in parseBody the int is a print flag to include "value"
typedef std::vector<std::pair<DbVar*,int>>dbs_type; // collect all the parsed vars here

// this is for the bits
typedef std::map<const char*, std::pair<DbVar_t*,int>, char_dcmp>bits_map;

// do conversion for large values unsigned ints
int32_t getInt32Val(DbVar *db);
int16_t getInt16Val(DbVar *db);

bool extractInt32Val(double &dval, DbVar *db);
bool extractInt16Val(double &dval, DbVar *db);


int addVarToCj(cJSON* cj, DbVar*db);
int addVarToCj(cJSON* cj, std::pair<DbVar*,int>dbp);
int addVarToCj(cJSON* cj, const char *dname);
int addVarToCj(cJSON* cj, DbVar* db, int flag);

typedef struct varList_t {
    varList_t(const char* iuri)
    {
        uri = iuri;
    };
    // TODO delete dbmap
    ~varList_t(){};
    const char* uri;
    dbvar_map dbmap;
} varList;

typedef std::map<std::string, varList*> dburi_map;

typedef struct sysCfg_t {

    sysCfg_t() :name(NULL), protocol(NULL), id(NULL), ip_address(NULL), p_fims(NULL)
    {
        cj = NULL;
        cjloaded = 0;
        debug = 0;
        scanreq = 0;
        unsol = -1;
        cjclass = NULL;
        base_uri = NULL;
        defUri = NULL;

        for (int i = 0; i < static_cast<int32_t>(Type_of_Var::NumTypes) ; i++)
        {
            useReadb[i] = false;
        }
    }
    ~sysCfg_t()
    {
        FPS_DEBUG_PRINT(" %s closing \n", __FUNCTION__);

        //if(name)free(name);
        if(protocol)free(protocol);
        if(id)free(id);
        if(ip_address)free(ip_address);
        //if (pub) free(pub);
        if (name) free(name);

        if (cj) cJSON_Delete(cj);
        cleanUpDbMaps();
    };

    public:
        // now always creates one. it is added to the uri map
        DbVar* newDbVar(std::string name, int type, int offset, char* uri, char* variation) 
        {
            DbVar* db = NULL;
            // TODO check this 
            //if (dbMap.find(name) == dbMap.end()){
                db = new DbVar(name, type, offset, uri, variation);
                dbMap[name] = db;
                db->idx = static_cast<int32_t>(dbVec[type].size());
                dbVec[type].push_back(db);
                if(dbMapIxs[type].find(offset) == dbMapIxs[type].end())
                {   
                    dbMapIxs[type][offset] = db;
                }
                else
                {
                    FPS_ERROR_PRINT(" %s name [%s] already defined in dbMapIx\n", __FUNCTION__, name.c_str());
                }
            //}
            //else
            //{
            //    FPS_ERROR_PRINT(" %s name [%s] already defined in dbMap\n", __FUNCTION__, name.c_str());
            //}
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
                if (db->linkback != NULL)
                {
                    if (db->linkb == NULL)
                    {
                        db->linkb = getDbVar(db->linkback);
                    }
                    if (db->linkb != NULL)
                    {
                        if (db->linkb->linkb == db)
                        {
                            FPS_ERROR_PRINT(" link loopback detected from [%s] to [%s] \n", db->name.c_str(), db->linkb->name.c_str());
                        }
                        else
                        {
                            setDb(db->linkb, fval);
                        }
                    }
                }
                db->initSet = 1;
                switch (db->type) 
                {
                    case Type_Analog:
                    case Type_AnalogOS:
                    {
                        // also copy valueint or the group30var5 stuff
                        db->valuedouble = fval;
                        //db->valueint =  static_cast<int32_t>(fval);

                        return 1;
                    }    
                    case AnF32:
                    {
                        db->valuedouble = fval;
                        return  1;
                    } 
                    case Type_Binary:
                    case Type_BinaryOS:
                    {
                        db->valuedouble = fval;
                        //db->valueint = static_cast<int32_t>(fval);
                        return  1;
                    } 
                    case AnIn32:
                    {
                        db->valuedouble = fval;
                        //db->valueint = static_cast<int32_t>(fval);
                        //db->anInt32 = db->valueint;
                        return  1;
                    }
                   case AnIn16:
                    {
                        db->valuedouble = fval;
                        //db->valueint = static_cast<int32_t>(fval);
                        //db->xnInt16 = db->valueint;
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
                if (db->linkback != NULL)
                {
                    if (db->linkb == NULL)
                    {
                        db->linkb = getDbVar(db->linkback);
                    }
                    if(db->linkb != NULL)
                    { 
                        if (db->linkb->linkb == db)
                        {
                            FPS_ERROR_PRINT(" link loopback detected from [%s] to [%s] \n", db->name.c_str(), db->linkb->name.c_str());
                        }
                        else
                        {
                            setDb(db->linkb, ival);
                        }
                    }
                }
                db->initSet = 1;

                switch (db->type) 
                {
                    case Type_Analog:
                    case Type_AnalogOS:
                    {
                        // also copy valueint or the group30var5 stuff
                        db->valuedouble = (double)ival;
                        //db->valueint =  ival;
                        return 1;
                    }    
                    case AnF32:
                    {
                        db->valuedouble = (double)ival;
                        return  1;
                    } 
                    case Type_Binary:
                    case Type_BinaryOS:
                    {
                        db->valuedouble = (double)ival;
                        //db->valueint = ival;
                        return  1;
                    } 
                    case AnIn32:
                    {
                        FPS_ERROR_PRINT(" setting the AnIn32 int value of [%s] %s to %d sign %d \n", db->name.c_str(), iotypToStr(db->type), ival, db->sign );                
                        db->valuedouble = (double)ival;
                        //db->valueint = ival;
                        //db->anInt32 = ival;
                        return  1;
                    }
                   case AnIn16:
                    {
                        db->valuedouble = (double)ival;
                        //db->valueint = ival;
                        //db->xnInt16 = ival;
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

        int setDbVar(DbVar* db, cJSON* cj)
        {
            if(db != NULL)
            {
                switch (db->type) 
                {
                    case Type_AnalogOS:
                    case Type_Analog:
                    case AnF32:
                    {
                        return setDb(db, cj->valuedouble);
                    }    
                    case Type_BinaryOS:
                    case Type_Binary:
                    case AnIn32:
                    case AnIn16:
                    {
                        FPS_ERROR_PRINT(" XXX setting the value of [%s] %s to  (double) %f sign %d \n", db->name.c_str(), iotypToStr(db->type), cj->valuedouble, db->sign );                

                        return setDb(db, cj->valuedouble);
                        //return setDb(db, cj->valueint);
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

        int setDbVar(const char* name, cJSON* cj)
        {
            return setDbVar(getDbVar(name),cj);
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

        int setDbVarIx(int dbtype, int idx, double ival)
        {
            DbVar* db = NULL;
            if(idx < static_cast<int32_t>(dbVec[dbtype].size()))
            {   
                db = dbVec[dbtype][idx];
            }
            if (db != NULL)
            {
                FPS_ERROR_PRINT(" setting the double value of [%s] %s to %f sign %d \n", db->name.c_str(), iotypToStr(dbtype), ival, db->sign );                
                return setDb(db, ival);
            }
            else
            {
                FPS_ERROR_PRINT("  %s Set Double Variable %s index  %f unknown \n", __FUNCTION__, iotypToStr(dbtype), ival);                  
            }

            return 0;
        };

        int setDbVarIx(int dbtype, int idx, int ival)
        {
            DbVar* db = NULL;
            if(idx < static_cast<int32_t>(dbVec[dbtype].size()))
            {   
                db = dbVec[dbtype][idx];
            }
            if (db != NULL)
            {
                FPS_ERROR_PRINT(" setting the int value of [%s] %s to %d sign %d \n", db->name.c_str(), iotypToStr(dbtype), ival, db->sign );                
                return setDb(db, ival);
            }
            else
            {
                FPS_ERROR_PRINT(" %s Set INT Variable %s index  %d unknown \n", __FUNCTION__, iotypToStr(dbtype), ival);                  
            }
            
            return 0;
        };

        int getDbIdx(int dbtype, const char* name)
        {
            DbVar* db = getDbVar(name);
            if ((db != NULL) && (db->type == dbtype))
                return db->idx;
            return -1;
        };

        DbVar* getDbVarId(int dbtype, int idx)
        {
            if(idx < static_cast<int32_t>(dbVec[dbtype].size()))
            {   
                return dbVec[dbtype][idx];
            }
            return NULL;
        };

        void setupReadb(int who)
        {
            if (who == DNP3_OUTSTATION)
            {
               useReadb[AnF32] = true;
               useReadb[AnIn16] = true;
               useReadb[AnIn32] = true;
               useReadb[Type_Crob] = true;
            }
            else // master
            {
                useReadb[Type_Analog] = true;
                useReadb[Type_Binary] = true;
            }
            
        }
        // dbvar_map dbMap;
        // dbix_map dbMapIx[Type_of_Var::NumTypes];
        // duri_map uriMap;  it->first is from a strdup it->second is a vector
        // bits_map bitsMap;
        void cleanUpDbMaps()
        {
            FPS_ERROR_PRINT(" %s DbVars===> \n\n", __FUNCTION__);
            {
                dbvar_map::iterator it;
                for (it = dbMap.begin(); it != dbMap.end(); ++it)
                {
                    DbVar* db = it->second;
                    FPS_ERROR_PRINT(" name :[%s] Type :[%d] offset : [%d] ===> \n"
                                , it->first.c_str()
                                , db->type
                                , db->offset
                                );
                    delete db;
                }
            }
            FPS_ERROR_PRINT(" %s DbVars<=== \n\n", __FUNCTION__);
            dbMap.clear();
            for (int i = 0; i < static_cast<int32_t>(Type_of_Var::NumTypes); i++)
            {
                // for (int j = 0; j < static_cast<int32_t>(dbMapIx[i].size()); j++)
                // {
                //     free((void *)dbMapIx[i][j].first);
                // }
                dbVec[i].clear();
            }
            {
                duri_map::iterator it;
                for (it = uriMap.begin(); it != uriMap.end(); ++it)
                {
                    free((void *)it->first);
                    it->second.clear();
                }
                uriMap.clear();
            }
        }

        void showDbMap()
        {
            FPS_ERROR_PRINT(" %s DbVars===> \n\n", __FUNCTION__);
            for (int i = 0; i < static_cast<int32_t>(Type_of_Var::NumTypes); i++)
            {
                if (dbVec[i].size() > 0)
                {
                    FPS_ERROR_PRINT(" dnp3 type [%s]\n", iotypToStr(i)); 
                    for (int j = 0; j < static_cast<int32_t>(dbVec[i].size()); j++)
                    {
                        DbVar* db = dbVec[i][j];
                        if(db != NULL)
                        {
                            FPS_ERROR_PRINT(" idx [%d.%d] ->name :[%s] offset : [%d] ===> \n"
                                , db->type
                                , db->idx
                                , db->name.c_str()
                                , db->offset
                                );
                        }
                    }
                }
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

        void addVarsToVec(std::vector<DbVar*>& dbs, const char* uri)
        {
            dbvar_map::iterator it;
            //int flag = 0;
            for (it = dbMap.begin() ; it != dbMap.end();++it)
            {
                DbVar* db = it->second;
                int dummy;
                if(confirmUri(db, uri, dummy))
                {
                    dbs.push_back(db);
                    if(debug ==1 )
                        FPS_ERROR_PRINT("added to Vector name: %s => Type: %d offset : %d\n", it->first.c_str(), db->type, db->offset);
                }
            }
        }

        void addVarsToVec(dbs_type& dbs, const char* uri)
        {
            dbvar_map::iterator it;
            int flag = 0;
            for (it = dbMap.begin() ; it != dbMap.end();++it)
            {
                DbVar* db = it->second;
                int dummy;
                if(confirmUri(db, uri, dummy))
                {
                    dbs.push_back(std::make_pair(db, flag));
                    if(debug ==1 )
                        FPS_ERROR_PRINT("added to Vector name: %s => Type: %d offset : %d\n", it->first.c_str(), db->type, db->offset);
                }
            }
        }

        void showUris()
        {
            FPS_ERROR_PRINT(" %s uris===> \n\n", __FUNCTION__);

            duri_map::iterator it;
            for (it = uriMap.begin(); it != uriMap.end(); ++it)
            {
                FPS_ERROR_PRINT(" %s uri [%s] num vars %d\n", __FUNCTION__, it->first, static_cast<int32_t>(it->second.size()));
                for (int i = 0 ; i < static_cast<int32_t>(it->second.size()); i++ )
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

        //typedef std::map<std::string, varList*> dburi_map;
        //typedef std::map<std::string, DbVar_t*> dbvar_map;
//        typedef struct varList_t {
//           varList(const char* iuri){
//           uri = iuri;
//         };
//     // TODO delete dbmap
//        ~varList(){};
//        const char* uri;
//        dbvar_map dbmap;
//      } varList;
        void showNewUris()
        {
            FPS_ERROR_PRINT(" %s New uris===> \n\n", __FUNCTION__);

            dburi_map::iterator it;
            dbvar_map::iterator itd;
            for (it = dburiMap.begin(); it != dburiMap.end(); ++it)
            {
                FPS_ERROR_PRINT(" %s uri [%s] num vars %d\n", __FUNCTION__, it->first.c_str(), static_cast<int32_t>(it->second->dbmap.size()));

                // dbvar_map
                auto dvar = it->second;
                auto dbm = dvar->dbmap;
                for (itd = dbm.begin(); itd != dbm.end(); ++itd)
                {
                    FPS_ERROR_PRINT(" %s var [%s] \n", __FUNCTION__, itd->first.c_str());
                    //DbVar* db = it->second[i];
                    // FPS_ERROR_PRINT("                 [%s] %d %d\n"
                    //             , db->name.c_str() 
                    //             , db->type
                    //             , db->offset
                    //             );
                }
            }
            FPS_ERROR_PRINT(" %s<=== New uris \n\n", __FUNCTION__);
        }

        // make sure the uri is in the list
        // what about default / base uri 
        //
        bool confirmUri(DbVar* db, const char*uri, int& nfrags)
        {
            // first limit the uri 
            if(debug)
                FPS_DEBUG_PRINT(" %s uris===> \n", __FUNCTION__);

            duri_map::iterator it;
            for (it = uriMap.begin(); it != uriMap.end(); ++it)
            {
                // it.first is the uri
                if(debug)
                    FPS_DEBUG_PRINT(" %s uris checking [%s] uri [%s] \n ", __FUNCTION__, it->first, uri);

                if(strncmp(it->first, uri, strlen(it->first)) == 0)
                {
                    nfrags = countUris(it->first);                   
                    if(db != NULL)
                    {
                        if(debug)
                            FPS_DEBUG_PRINT(" %s possible uri match [%s] num vars %d\n", __FUNCTION__, it->first, static_cast<int32_t>(it->second.size()));
                        for (int i = 0 ; i < static_cast<int32_t>(it->second.size()); i++ )
                        {
                            if(it->second[i] == db)
                            {
                                if(debug)
                                    FPS_DEBUG_PRINT(" URI Match                [%s] %d %d\n"
                                                , db->name.c_str() 
                                                , db->type
                                                , db->offset
                                                );
                                return true;
                            }
                        }
                    }
                    else
                    {
                        return true;
                    }                  
                }
            }
            if(debug)
                FPS_DEBUG_PRINT(" %s<=== uris \n\n", __FUNCTION__);
            return false;
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

        bool checkUris(int who)
        {
            duri_map::iterator it;
            for (it = uriMap.begin(); it != uriMap.end(); ++it)
            {
                for (int i = 0; i < static_cast<int32_t>(it->second.size()); i++ )
                {
                    DbVar* db = it->second[i];
                    if (db->initSet == 0)
                    {
                        if(who == DNP3_OUTSTATION)
                        {
                            if ((db->type == Type_Analog) || (db->type == Type_Binary))
                            {
                                FPS_ERROR_PRINT(" %s init missing on variable [%s]\n", __FUNCTION__, db->name.c_str());
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
                                FPS_ERROR_PRINT(" %s init missing on variable [%s]\n", __FUNCTION__, db->name.c_str());
                                return false;
                            }
                        }
                    }
                }
            }
            return true;
        };

        int getSubs(const char**subs, int num, int who)
        {
            if (num < static_cast<int32_t>(uriMap.size()))
            {
                return uriMap.size();
            }
            int idx = 0;
            subs[idx++] = base_uri;
            duri_map::iterator it;
            for (it = uriMap.begin(); it != uriMap.end(); ++it)
            {
                subs[idx++]=it->first;
            }
            return idx;
        }

        // only get the ones for vars applied to this application (outstation or master)
        int getUris(int who)
        {
            //if(debug ==1)
                FPS_ERROR_PRINT(" %s uris===>%d<=== \n\n", __FUNCTION__, who);

            duri_map::iterator it;
            for (it = uriMap.begin(); it != uriMap.end(); ++it)
            {
                char replyto[1024];
                char getUri[1024];
                //sprintf(replyto, "%s/reply%s", server_map->base_uri, it->first);
                //server_map->p_fims->Send("get", it->first, replyto, NULL);
                if (it->first[0] == '/') 
                {
                    snprintf(replyto, sizeof(replyto),"%s/reply%s",  base_uri, it->first);
                    snprintf(getUri,sizeof(getUri),"%s", it->first);
                } 
                else
                {
                    snprintf(replyto, sizeof(replyto),"%s/reply%s",  base_uri, it->first);
                    snprintf(getUri,sizeof(getUri),"%s", it->first);
                }
                //if(debug == 1)
                    FPS_ERROR_PRINT(" uri : [%s] replyto: [%s]\n"
                        , getUri
                        , replyto
                        );
        
                p_fims->Send("get", getUri, replyto, NULL);
            }
            //if(debug ==1)
                FPS_ERROR_PRINT(" %s<=== get uris DONE\n\n", __FUNCTION__);
            return 0;
        }

        // new way of doing this
        // new structure varList  uri name, map of dbVars
        // struct varList {
        //    const char* uri;
        //    dbvar_map dbmap;
        //};
        //typedef std::map<std::string, varList*> dburi_map;
        //typedef std::map<std::string, DbVar_t*> dbvar_map;
//      typedef struct varList_t {
//           varList(const char* iuri){
//           uri = iuri;
//         };
//     // TODO delete dbmap
//        ~varList(){};
//        const char* uri;
//        dbvar_map dbmap;
//      } varList;

        void addDbUri(const char *uri, DbVar*db)
        {
            std::string mapUri = uri;

            // this is a pointer to the uri 
            // if there is not one in the map then create a new one and then add it
            //duri_map::iterator it_uris;
            if(dburiMap.find(uri) == dburiMap.end())
            {

                FPS_ERROR_PRINT("     %s  ==> ADDED varlist [%s]  dburi size %d \n", __FUNCTION__, uri, (int) dburiMap.size());
                //dbvar_map dvar;
                dburiMap[mapUri] = new varList_t(uri); 
            }
            else
            {

                FPS_ERROR_PRINT("     %s  ==> FOUND varlist [%s]  dburi size %d \n", __FUNCTION__, uri, (int) dburiMap.size());
            }
            //mymap.insert ( std::pair<char,int>('a',100) );
            dbvar_map dbm = dburiMap[mapUri]->dbmap;
            dbm.insert(std::pair<string,DbVar*>(db->name, db));
            
            //if(debug ==1)
                FPS_ERROR_PRINT(" %s  ==> added var [%s]  dbm size %d \n", __FUNCTION__, db->name.c_str(), (int) dbm.size());

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
            if(uri && uri->valuestring)
            {
                return addUri(uri->valuestring, db);
            }
        }


        char*getDefUri(int who)
        {
            if(defUri == NULL)
            {
                if(who == DNP3_MASTER)
                {
                    asprintf(&defUri,"/components/%s",id);
                }
                else
                {
                    asprintf(&defUri,"/interfaces/%s",id);
                }
            }
            return defUri;
        }   
        
        char* name;
        char* protocol;
        char* id;
        char* ip_address;
        char *defUri;
        //char* pub;
        int version;
        int port;
        int local_address;
        int remote_address;
        int frequency;
        int idx;
        char* uri;

        // new way of doing this
        dbvar_map dbMap;
        dbvec    dbVec[Type_of_Var::NumTypes];
        dbix_map dbMapIxs[Type_of_Var::NumTypes];
        duri_map uriMap;
        bits_map bitsMap;
        dburi_map dburiMap;


        int numObjs[Type_of_Var::NumTypes];
        bool useReadb[Type_of_Var::NumTypes]; // set true if writes to this type should be diverted to readb if setup

        fims* p_fims;
        cJSON* cj;  
        int cjloaded;
        int debug;
        int scanreq;     //used to request a class 1, 2 or 3 scan 
        int unsol;       // set to 1 to allow unsol in oustation
        cJSON* cjclass;  // used to change class of a var in outstation
        char* base_uri;        
} sysCfg;

DbVar* getDbVar(sysCfg *sysdb, const char *name);

//bool process_dnp3_message(int bytes_read, int header_length, datalog* data, system_config* config, server_data* server_map, bool serial, uint8_t * query);
//bool update_register_value(dnp3_mapping_t* map, bool* reg_type, maps** settings, cJSON* value);
void emit_event(fims* pFims, const char* source, const char* message, int severity);
cJSON* get_config_json(int argc, char* argv[]);
//server_data* create_register_map(cJSON* registers, datalog* data);
//int parse_system(cJSON *system, system_config *config);

// new mapping
bool parse_system(cJSON* object, sysCfg* sys, int who);
bool parse_variables(cJSON* object, sysCfg* sys, int who);
cJSON *parseJSONConfig(char *file_path);
void addCjTimestamp(cJSON *cj, const char* ts);
void pubWithTimeStamp(cJSON *cj, sysCfg* sys, const char* ev);
void pubWithTimeStamp2(cJSON *cj, sysCfg* sys, const char* ev);

void sysdbAddtoRecord(sysCfg* sysdb,const char* field, const AnalogOutputInt16& cmd, uint16_t index);
void sysdbAddtoRecord(sysCfg* sysdb,const char* field, const AnalogOutputInt32& cmd, uint16_t index);
void sysdbAddtoRecord(sysCfg* sysdb,const char* field, const AnalogOutputFloat32& cmd, uint16_t index);
void sysdbAddtoRecord(sysCfg* sysdb,const char* field, const char* cmd, uint16_t index);
const char* cfgGetSOEName(sysCfg* sysdb, const char* fname);

int addVarToCj(cJSON* cj, DbVar*db);
int addVarToCj(sysCfg* sys, cJSON* cj, const char* dname);

cJSON* parseBody( dbs_type&dbs, sysCfg*sys, fims_message*msg, int who);
int addValueToVec(dbs_type&dbs, sysCfg*sys, /*CommandSet& commands,*/ const char* valuestring, cJSON *cjvalue, int flag);
// int addValueToDb(sysCfg*sys, const char* name , cJSON *cjvalue, int flag);
bool checkWho(sysCfg*sys, const char *name, int who);
bool checkWho(sysCfg*sys, DbVar *db, int who);
int getSysUris(sysCfg* sys, int who, const char **&subs, bool *&bpubs);

 
#endif
