/*
 * newOutStation.h
 */
#ifndef NEW_OUTSTATIONAPPLICATION_H
#define NEW_OUTSTATIONAPPLICATION_H

#include <openpal/executor/UTCTimestamp.h>

#include "opendnp3/app/Indexed.h"
#include "opendnp3/app/MeasurementTypes.h"
#include "opendnp3/app/parsing/ICollection.h"
#include "opendnp3/gen/AssignClassType.h"
#include "opendnp3/gen/PointClass.h"
#include "opendnp3/gen/RestartMode.h"
#include "opendnp3/link/ILinkListener.h"
#include "opendnp3/outstation/ApplicationIIN.h"
#include "opendnp3/outstation/IOutstationApplication.h"


#include <memory>
#include "dnp3_utils.h"
#include <iostream>

// class newCommandHandler final : public opendnp3::ICommandHandler
// {

// public:
//     newCommandHandler(sysCfg* myDB){cfgdb = myDB;};

//     static std::shared_ptr<ICommandHandler> Create(sysCfg* db)
//     {
//         return std::make_shared<newCommandHandler>(db);
//     }

//     void Start() override {}
//     void End() override {}
/**
 * A singleton 
 */

class newOutstationApplication final : public opendnp3::IOutstationApplication
{
public:
    newOutstationApplication(sysCfg* db) {cfgdb=db;};

    static std::shared_ptr<newOutstationApplication> Create(sysCfg* db)
    {
        return std::make_shared<newOutstationApplication>(db);
    }

    bool SupportsWriteAbsoluteTime()
    {
        std::cout << " newOutstationApplication -<"<< __FUNCTION__<<" called \n";
        return false;
    }
    sysCfg* cfgdb;

};
#endif