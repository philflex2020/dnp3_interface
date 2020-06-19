/*
 * author: pwilshire
 *     11 May, 2020
 * This code provides the details for a pub from the master of the componets returned in an outstation scan.
 * This pub is returned to the customer facing Fleet Manager Outstation
 * NOTE only analogs and binaries are handled at this stage.
 * 
 */
#include <cjson/cJSON.h>
#include <sstream>
#include "fpsSOEHandler.h"
#include "dnp3_utils.h"

using namespace std; 
using namespace opendnp3; 

namespace asiodnp3 { 

void fpsSOEHandler::Process(const HeaderInfo& info, const ICollection<Indexed<Binary>>& values) {
    static sysCfg *static_sysdb = sysdb;
    static int items = 0;
    if(static_sysdb->debug)
        FPS_DEBUG_PRINT(">> ******************************Bin:\n");
    auto print = [](const Indexed<Binary>& pair) {
        DbVar* db = static_sysdb->getDbVarId(Type_Binary, pair.index);
        if (db != NULL) 
        {
            const char* vname = db->name.c_str();// static_sysdb->getBinary(pair.index);
            if(static_sysdb->debug)
                FPS_DEBUG_PRINT("***************************** bin idx %d name [%s] value [%d]\n", pair.index, db->name.c_str(), pair.value.value);

            if(strcmp(vname,"Unknown")!= 0) 
            {
                static_sysdb->addPubVar(db, pair.value.value);
                static_sysdb->setDbVar(db->uri, vname, pair.value.value);
                items++;
            }
        }
        else
        {
            if(static_sysdb->debug)
                FPS_ERROR_PRINT("***************************** bin idx %d No Var Found\n", pair.index);
        }        
    };
    values.ForeachItem(print);
    if(static_sysdb->cj)
    {
        if(items > 0)
            static_sysdb->cjloaded++;
    }
    if(static_sysdb->debug)
        FPS_DEBUG_PRINT("<< ******************************Bin:\n" );
}

void fpsSOEHandler::Process(const HeaderInfo& info, const ICollection<Indexed<DoubleBitBinary>>& values) {
    if(sysdb->debug)
        FPS_DEBUG_PRINT("******************************DBin: \n");
    return PrintAll(info, values);
}

void fpsSOEHandler::Process(const HeaderInfo& /*info*/, const ICollection<Indexed<BinaryCommandEvent>>& values) {
    auto print = [](const Indexed<BinaryCommandEvent>& pair) {
        std::cout << "BinaryCommandEvent: "
                  << "[" << pair.index << "] : " << pair.value.time << " : " << pair.value.value << " : "
                  << CommandStatusToString(pair.value.status) << std::endl;
    };
    values.ForeachItem(print);
}

void fpsSOEHandler::Process(const HeaderInfo & /* info*/, const ICollection<Indexed<Analog>>& values) {
    // magic static
    static sysCfg *static_sysdb = sysdb;
    static int items = 0;
    if(static_sysdb->debug)
        FPS_DEBUG_PRINT(">> ******************************An:\n");
    auto print = [](const Indexed<Analog>& pair) {
        DbVar* db = static_sysdb->getDbVarId(Type_Analog, pair.index);
        if (db != NULL) 
        {
            const char* vname = db->name.c_str();// static_sysdb->getBinary(pair.index);
            if(static_sysdb->debug)
                FPS_ERROR_PRINT("***************************** analog idx %d name [%s] uri [%s] value [%f]\n", pair.index, db->name.c_str(), db->uri, pair.value.value);
            if(strcmp(vname,"Unknown") != 0) 
            {
                static_sysdb->addPubVar(db, pair.value.value);
                static_sysdb->setDbVar(db->uri, vname, pair.value.value);
                items++;
            }
        }
        else
        {
            if(static_sysdb->debug)
                FPS_ERROR_PRINT("***************************** analog idx %d No Var Found\n", pair.index);
        }
    };
    values.ForeachItem(print);
    if(static_sysdb->cj)
    {
        if(items > 0)
            static_sysdb->cjloaded++;
    }
    if(static_sysdb->debug)
        FPS_DEBUG_PRINT("<< ******************************An:\n");
    return;
}
void fpsSOEHandler::Process(const HeaderInfo& info, const ICollection<Indexed<Counter>>& values) {
    FPS_DEBUG_PRINT("******************************Cnt: \n");
    return PrintAll(info, values);
}
void fpsSOEHandler::Process(const HeaderInfo& info, const ICollection<Indexed<FrozenCounter>>& values) {
    FPS_DEBUG_PRINT("******************************CntFz: \n");
    return PrintAll(info, values);
}
void fpsSOEHandler::Process(const HeaderInfo& info, const ICollection<Indexed<BinaryOutputStatus>>& values) {
    FPS_DEBUG_PRINT("******************************BiOpSta: \n");
    return PrintAll(info, values);
}
void fpsSOEHandler::Process(const HeaderInfo& info, const ICollection<Indexed<AnalogOutputStatus>>& values) {
    FPS_DEBUG_PRINT("******************************AnOpSta: \n"); 
    std::cout << "AnalogOutputStatus: ";
    auto print = [](const Indexed<AnalogOutputStatus>& pair) {
            std::cout << "index [" << pair.index << "] : value ["  << pair.value.value << " ] "
                    //<< CommandStatusToString(pair.value.status) 
                    << std::endl;
    };
    values.ForeachItem(print);
}

//     return PrintAll(info, values);
// }
void fpsSOEHandler::Process(const HeaderInfo& /*info*/, const ICollection<Indexed<OctetString>>& values) {
    auto print = [](const Indexed<OctetString>& pair) {
        std::cout << "OctetString "
                  << " [" << pair.index << "] : Size : " << pair.value.ToRSlice().Size() << std::endl;
    };
    values.ForeachItem(print);
}
void fpsSOEHandler::Process(const HeaderInfo& /*info*/, const ICollection<Indexed<TimeAndInterval>>& values) {
    auto print = [](const Indexed<TimeAndInterval>& pair) {
        std::cout << "TimeAndInterval: "
                  << "[" << pair.index << "] : " << pair.value.time << " : " << pair.value.interval << " : "
                  << IntervalUnitsToString(pair.value.GetUnitsEnum()) << std::endl;
    };
    values.ForeachItem(print);
}

void fpsSOEHandler::Process(const HeaderInfo& /*info*/, const ICollection<Indexed<AnalogCommandEvent>>& values) {
    auto print = [](const Indexed<AnalogCommandEvent>& pair) {
        std::cout << "AnalogCommandEvent: "
                  << "[" << pair.index << "] : " << pair.value.time << " : " << pair.value.value << " : "
                  << CommandStatusToString(pair.value.status) << std::endl;
    };
    values.ForeachItem(print);
}
void fpsSOEHandler::Process(const HeaderInfo& /*info*/, const ICollection<Indexed<SecurityStat>>& values) {
    auto print = [](const Indexed<SecurityStat>& pair) {
        std::cout << "SecurityStat: "
                  << "[" << pair.index << "] : " << pair.value.time << " : " << pair.value.value.count << " : "
                  << static_cast<int>(pair.value.quality) << " : " << pair.value.value.assocId << std::endl;
    };
    values.ForeachItem(print);
}

void fpsSOEHandler::Process(const opendnp3::HeaderInfo& /*info*/,
                                 const opendnp3::ICollection<opendnp3::DNPTime>& values) {
    auto print = [](const DNPTime& value) { std::cout << "DNPTime: " << value.value << std::endl; };
    values.ForeachItem(print);
}
} // namespace asiodnp3
