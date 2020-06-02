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
#ifndef NEW_FPSMASTERAPPLICATION_H
#define NEW_FPSMASTERAPPLICATION_H

#include <opendnp3/master/IMasterApplication.h>

#include <memory>

#include <iostream>

#include "dnp3_utils.h"

// NOTE
// this starts and stops the pub for the eventthe newSOE handler 
// The FM master pubs the results of the outstation scan
// this will have to be picked up by the Fleet Manager outstation and sent back to the ERCOT system

class fpsMasterApplication : public opendnp3::IMasterApplication
{
public:
    fpsMasterApplication(sysCfg* fpsDB){sysdb = fpsDB;};

    static std::shared_ptr<IMasterApplication> Create(sysCfg* fpsDB)
    {
        return std::make_shared<fpsMasterApplication>(fpsDB);
    }

    virtual void OnReceiveIIN(const opendnp3::IINField& iin) override final 
    {
        if(sysdb->debug == 1)
            FPS_ERROR_PRINT ("Running [%s] IINField %u.%u \n", __FUNCTION__, iin.MSB, iin.LSB);
    }
    // start the pub object for the master.
    // 
    // TODO find a way to detect the scan result. I dont think we'll have more that one of these running at a time
    // newSOEHandler populates the cJSON object
    virtual void OnTaskStart(opendnp3::MasterTaskType type, opendnp3::TaskId id) override final 
    {
        if(sysdb->debug == 1)
            std::cout << "Running ["<<__FUNCTION__<<" TaskID :"<< id.GetId() << " Task Type :"<< MasterTaskTypeToString(type) <<"]\n";
        sysdb->cj = cJSON_CreateObject();
        sysdb->cjloaded = 0; 
    }
    // if cjloaded is well loaded then send out the pub
    virtual void OnTaskComplete(const opendnp3::TaskInfo& info) override final {
        //std::cout << "Running ["<<__FUNCTION__<<" TaskID :"<< id << " Task Type :"<< type <<"]\n";
        if(sysdb->debug == 1)
            std::cout << "Running ["<<__FUNCTION__<<"]\n";    //Code for adding timestamp
        if(sysdb->cj)
        {
            if (sysdb->cjloaded > 0) 
            {
                pubWithTimeStamp2(sysdb->cj, sysdb, "components");
                sysdb->cjloaded = 0;
            }
            cJSON_Delete(sysdb->cj);
            sysdb->cj = NULL;
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
        //std::cout << "Running ["<<__FUNCTION__<<"] status ["<<LinkStatusToString(value)<<"]\n";
        //cJSON* cj = cJSON_CreateObject();
        //cJSON_AddStringToObject(cj, "LinkStatus", LinkStatusToString(value));
        //pubWithTimeStamp(cj, sysdb,"StateChange");
        //cJSON_Delete(cj);
        //cj = NULL;
        std::string cstate = LinkStatusToString(value);
        char message[1024];
        snprintf(message, sizeof(message), "DNP3  %s Link Status Change [%s]\n"
                    , sysdb->id 
                    , cstate.c_str()
                    );
        emit_event(sysdb->p_fims, "DNP3", message, 1);
    }    
    
    sysCfg* sysdb;

};

#endif