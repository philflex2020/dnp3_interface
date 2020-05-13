/*
 * Copyright 2013-2019 Automatak, LLC
 *
 * Licensed to Green Energy Corp (www.greenenergycorp.com) and Automatak
 * LLC (www.automatak.com) under one or more contributor license agreements.
 * See the NOTICE file distributed with this work for additional information
 * regarding copyright ownership. Green Energy Corp and Automatak LLC license
 * this file to you under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may obtain
 * a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <cjson/cJSON.h>
#include <sstream>
#include "newSOEHandler.h"
#include "dnp3_utils.h"

// dont like this so dont do it
//extern sysCfg *xcfgdb;
using namespace std; 
using namespace opendnp3; 

namespace asiodnp3 { 

void newSOEHandler::Process(const HeaderInfo& info, const ICollection<Indexed<Binary>>& values) {
    std::cout << "******************************Bin: " <<std::endl;
    static sysCfg *static_cfgdb = cfgdb;
    static cJSON *cj; 
    static int first = 1;
    auto print = [](const Indexed<Binary>& pair) {
        if(first == 1)
        {
            cj = cJSON_CreateObject();
            first = 0;
        }
        char* vname = static_cfgdb->getBinary(pair.index);
        if(strcmp(vname,"Unknown")!= 0) 
        {
            cJSON_AddNumberToObject(cj, vname, pair.value.value);
        }
    };
    values.ForeachItem(print);
    first = 1;
    //Code for adding timestamp

    if(cj)
    {
        if(static_cfgdb->cj)
        {
            cJSON_AddItemToObject(static_cfgdb->cj, "binaries", cj);
        }
        else
        {
            pubWithTimeStamp(cj, static_cfgdb,"binaries");
            cJSON_Delete(cj);

        }
        cj = NULL;        
    }
    //std::cout << "******************************An: <<" <<std::endl;
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
    static cJSON *cj; 
    static int first = 1;
    auto print = [](const Indexed<Analog>& pair) {
        if(first == 1)
        {
            cj = cJSON_CreateObject();
            first = 0;
        }
        char* vname = static_cfgdb->getAnalog(pair.index);
        if(strcmp(vname,"Unknown")!= 0) 
        {
            cJSON_AddNumberToObject(cj, vname, (float)pair.value.value);
        }
    };
    values.ForeachItem(print);
    first = 1;
    if(cj)
    {
        if(static_cfgdb->cj)
        {
            cJSON_AddItemToObject(static_cfgdb->cj, "analogs", cj);
        }
        else
        {
            pubWithTimeStamp(cj, static_cfgdb, "analogs");
            cJSON_Delete(cj);
        }
        cj = NULL;        
    }
    //std::cout << "******************************An: <<" <<std::endl;
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
