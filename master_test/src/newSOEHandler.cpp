/*
 * author: pwilshire
 *     11 May, 2020
 * This code provides the details for a pub from the master of the componets returned in an outstation scan.
 * This pub is returned to the customer facing Fleet Manager Outstation
 * NOTE only analogs and binaries are reported at this stage.
 * 
 */
#include <cjson/cJSON.h>
#include <sstream>
#include "newSOEHandler.h"
#include "dnp3_utils.h"

using namespace std; 
using namespace opendnp3; 

namespace asiodnp3 { 

void newSOEHandler::Process(const HeaderInfo& info, const ICollection<Indexed<Binary>>& values) {
    std::cout << "******************************Bin: " <<std::endl;
    static sysCfg *static_cfgdb = cfgdb;
    //static cJSON *cj; 
    //static int first = 1;
    // start a cjson object for this collecion of items
    auto print = [](const Indexed<Binary>& pair) {
        // if(first == 1)
        // {
        //     //cj = cJSON_CreateObject();
        //     first = 0;
        // }
        DbVar* db = static_cfgdb->getDbVarId(Type_Binary, pair.index);
        if (db != NULL) 
        {
            const char* vname = db->name.c_str();// static_cfgdb->getBinary(pair.index);
            printf("***************************** bin idx %d name [%s] value [%d]\n", pair.index, db->name.c_str(), pair.value.value);

            if(strcmp(vname,"Unknown")!= 0) 
            {
                cJSON_AddBoolToObject(static_cfgdb->cj, vname, pair.value.value);
            }
        }
        else
        {
            printf("***************************** bin idx %d No Var Found\n", pair.index);
        }
        
    };
    values.ForeachItem(print);
    //first = 1;
    //if(cj)
    //{
        if(static_cfgdb->cj)
        {
            //cJSON_AddItemToObject(static_cfgdb->cj, cfgGetSOEName(static_cfgdb,"binaries"), cj);
            static_cfgdb->cjloaded++;
        }
        // else 
        // {
        //     cJSON_Delete(cj);
        // }
        //cj = NULL;        
    //}
    std::cout << "******************************Bin: <<" <<std::endl;
}

void newSOEHandler::Process(const HeaderInfo& info, const ICollection<Indexed<DoubleBitBinary>>& values) {
    std::cout << "******************************DBin: " <<std::endl;
    return PrintAll(info, values);
    
}

void newSOEHandler::Process(const HeaderInfo& /*info*/, const ICollection<Indexed<BinaryCommandEvent>>& values) {
    auto print = [](const Indexed<BinaryCommandEvent>& pair) {
        std::cout << "BinaryCommandEvent: "
                  << "[" << pair.index << "] : " << pair.value.time << " : " << pair.value.value << " : "
                  << CommandStatusToString(pair.value.status) << std::endl;
    };
    values.ForeachItem(print);
}

void newSOEHandler::Process(const HeaderInfo & /* info*/, const ICollection<Indexed<Analog>>& values) {
    // magic static
    static sysCfg *static_cfgdb = cfgdb;
    //static cJSON *cj; 
    //static int first = 1;
    // start a cjson object for this collecion of items
    auto print = [](const Indexed<Analog>& pair) {
        // if(first == 1)
        // {
        //     cj = cJSON_CreateObject();
        //     first = 0;
        // }
        DbVar* db = static_cfgdb->getDbVarId(Type_Analog, pair.index);
        if (db != NULL) 
        {
            const char* vname = db->name.c_str();// static_cfgdb->getBinary(pair.index);
            printf("***************************** analog idx %d name [%s] value [%f]\n", pair.index, db.c_str(), pair.value.value);

            if(strcmp(vname,"Unknown")!= 0) 
            {
                cJSON_AddNumberToObject(static_cfgdb->cj, vname, pair.value.value);
            }
        }
        else
        {
            printf("***************************** analog idx %d No Var Found\n", pair.index);
        }
    };
    values.ForeachItem(print);
    //first = 1;
    // at the end of this scan add the collection object to the main syscfg cjosn pub object
    //if(cj)
    //{
    if(static_cfgdb->cj)
    {
        //cJSON_AddItemToObject(static_cfgdb->cj, cfgGetSOEName(static_cfgdb,"analogs"), cj);
        static_cfgdb->cjloaded++;
    }
        // else 
        // {
        //     cJSON_Delete(cj);
        // }
        // cj = NULL;        
    //}
    std::cout << "******************************An: <<" <<std::endl;
    return;
}
void newSOEHandler::Process(const HeaderInfo& info, const ICollection<Indexed<Counter>>& values) {
    std::cout << "******************************Cnt: " <<std::endl;
    return PrintAll(info, values);
}
void newSOEHandler::Process(const HeaderInfo& info, const ICollection<Indexed<FrozenCounter>>& values) {
    std::cout << "******************************CntFz: " <<std::endl;
    return PrintAll(info, values);
}
void newSOEHandler::Process(const HeaderInfo& info, const ICollection<Indexed<BinaryOutputStatus>>& values) {
    std::cout << "******************************BiOpSta: " <<std::endl;
    return PrintAll(info, values);
}
void newSOEHandler::Process(const HeaderInfo& info, const ICollection<Indexed<AnalogOutputStatus>>& values) {
    std::cout << "******************************AnOpSta: " <<std::endl;
    return PrintAll(info, values);
}
void newSOEHandler::Process(const HeaderInfo& /*info*/, const ICollection<Indexed<OctetString>>& values) {
    auto print = [](const Indexed<OctetString>& pair) {
        std::cout << "OctetString "
                  << " [" << pair.index << "] : Size : " << pair.value.ToRSlice().Size() << std::endl;
    };
    values.ForeachItem(print);
}
void newSOEHandler::Process(const HeaderInfo& /*info*/, const ICollection<Indexed<TimeAndInterval>>& values) {
    auto print = [](const Indexed<TimeAndInterval>& pair) {
        std::cout << "TimeAndInterval: "
                  << "[" << pair.index << "] : " << pair.value.time << " : " << pair.value.interval << " : "
                  << IntervalUnitsToString(pair.value.GetUnitsEnum()) << std::endl;
    };
    values.ForeachItem(print);
}

void newSOEHandler::Process(const HeaderInfo& /*info*/, const ICollection<Indexed<AnalogCommandEvent>>& values) {
    auto print = [](const Indexed<AnalogCommandEvent>& pair) {
        std::cout << "AnalogCommandEvent: "
                  << "[" << pair.index << "] : " << pair.value.time << " : " << pair.value.value << " : "
                  << CommandStatusToString(pair.value.status) << std::endl;
    };
    values.ForeachItem(print);
}
void newSOEHandler::Process(const HeaderInfo& /*info*/, const ICollection<Indexed<SecurityStat>>& values) {
    auto print = [](const Indexed<SecurityStat>& pair) {
        std::cout << "SecurityStat: "
                  << "[" << pair.index << "] : " << pair.value.time << " : " << pair.value.value.count << " : "
                  << static_cast<int>(pair.value.quality) << " : " << pair.value.value.assocId << std::endl;
    };
    values.ForeachItem(print);
}

void newSOEHandler::Process(const opendnp3::HeaderInfo& /*info*/,
                                 const opendnp3::ICollection<opendnp3::DNPTime>& values) {
    auto print = [](const DNPTime& value) { std::cout << "DNPTime: " << value.value << std::endl; };
    values.ForeachItem(print);
}
} // namespace asiodnp3
