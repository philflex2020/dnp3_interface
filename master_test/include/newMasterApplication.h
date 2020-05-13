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

// this starts and stops the pub for the event 

class newMasterApplication : public opendnp3::IMasterApplication
{
public:
    newMasterApplication(sysCfg* myDB){cfgdb = myDB;};

    static std::shared_ptr<IMasterApplication> Create(sysCfg* myDB)
    {
        return std::make_shared<newMasterApplication>(myDB);
    }


    virtual void OnReceiveIIN(const opendnp3::IINField& iin) override final {}

    virtual void OnTaskStart(opendnp3::MasterTaskType type, opendnp3::TaskId id) override final {
        std::cout << "Running ["<<__FUNCTION__<<" TaskID :"<< id << " Task Type :"<< type <<"]\n";
        cfgdb->cj = cJSON_CreateObject();
    }

    virtual void OnTaskComplete(const opendnp3::TaskInfo& info) override final {
        //std::cout << "Running ["<<__FUNCTION__<<" TaskID :"<< id << " Task Type :"<< type <<"]\n";
        std::cout << "Running ["<<__FUNCTION__<<"]\n";//Code for adding timestamp
        if(cfgdb->cj)
        {
            addCjTimestamp(cj, "Timestamp");
            char *out = cJSON_PrintUnformatted(cj);
            cJSON_Delete(cj);
            cfgdb->cj = NULL;
            if (out) 
            {
                char tmp[1024];
                snprintf(tmp,1024,"/mypub/%s/%s", "id", cfgdb->id);

                cfgdb->p_fims->Send("pub", tmp, NULL, out);
                free(out);
            }
            else
            {
                std::cout << "***************error in Application pub : <<" <<std::endl;
            }
    }
    }

    virtual bool AssignClassDuringStartup() override final
    {
        return false;
    }

    virtual void ConfigureAssignClassRequest(const opendnp3::WriteHeaderFunT& fun) override final {}

    virtual openpal::UTCTimestamp Now() override final;

    virtual void OnStateChange(opendnp3::LinkStatus value) override final {}
    sysCfg* cfgdb;

};

#endif