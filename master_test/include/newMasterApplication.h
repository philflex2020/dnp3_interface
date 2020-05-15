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
#ifndef NEW_DEFAULTMASTERAPPLICATION_H
#define NEW_MASTERAPPLICATION_H

#include <opendnp3/master/IMasterApplication.h>

#include <memory>

#include <iostream>

#include "dnp3_utils.h"

// NOTE
// this starts and stops the pub for the eventthe newSOE handler 
// The FM master pubs the results of the outstation scan
// this will have to be picked up by the Fleet Manager outstation and sent back to the ERCOT system

class newMasterApplication : public opendnp3::IMasterApplication
{
public:
    newMasterApplication(sysCfg* myDB){cfgdb = myDB;};

    static std::shared_ptr<IMasterApplication> Create(sysCfg* myDB)
    {
        return std::make_shared<newMasterApplication>(myDB);
    }

    virtual void OnReceiveIIN(const opendnp3::IINField& iin) override final {}
    // start the pub object.
    // TODO find a way to detect the scan result. I dont think we'll have more that one of these running at a time
    // newSOEHandler populates the cJSON object
    virtual void OnTaskStart(opendnp3::MasterTaskType type, opendnp3::TaskId id) override final {
        std::cout << "Running ["<<__FUNCTION__<<" TaskID :"<< id.GetId() << " Task Type :"<< MasterTaskTypeToString(type) <<"]\n";
        cfgdb->cj = cJSON_CreateObject();
        cfgdb->cjloaded = 0; 
    }

    virtual void OnTaskComplete(const opendnp3::TaskInfo& info) override final {
        //std::cout << "Running ["<<__FUNCTION__<<" TaskID :"<< id << " Task Type :"<< type <<"]\n";
        std::cout << "Running ["<<__FUNCTION__<<"]\n";//Code for adding timestamp
        if(cfgdb->cj)
        // TODO fix interfaces name
        {
            if (cfgdb->cjloaded > 0) 
            {
                pubWithTimeStamp(cfgdb->cj, cfgdb, "interfaces");
                cfgdb->cjloaded = 0;
            }
            cJSON_Delete(cfgdb->cj);
            cfgdb->cj = NULL;
        }  
    }

    virtual bool AssignClassDuringStartup() override final
    {
        return false;
    }

    virtual void ConfigureAssignClassRequest(const opendnp3::WriteHeaderFunT& fun) override final {}

    virtual openpal::UTCTimestamp Now() override final;

    virtual void OnStateChange(opendnp3::LinkStatus value) override final 
    {  
        std::cout << "Running ["<<__FUNCTION__<<"] status ["<<LinkStatusToString(value)<<"]\n";
        cJSON* cj = cJSON_CreateObject();
        cJSON_AddStringToObject(cj, "LinkStatus", LinkStatusToString(value));
        pubWithTimeStamp(cj, cfgdb,"StateChange");
        cJSON_Delete(cj);
        cj = NULL;
    }    
    
    sysCfg* cfgdb;

};

#endif