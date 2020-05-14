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

#include <memory>
#include "dnp_utils.h"
#include <iostream>

/**
 * A singleton 
 */
class newOutstationApplication : public IOutstationApplication
{
public:
    static std::shared_ptr<IOutstationApplication> Create(sysCfg *db)
    {
        return std::make_shared<newOutstationApplication>(db);
    }

    newOutstationApplication(sysCfg* db) {
        cfgdb=db;
    };
    bool SupportsWriteAbsoluteTime()
    {
        std::cout << " newOutstationApplication -<"<< __FUNCTION__<<" called \n";
        return false;
    }
    sysCfg* cfgdb;

};
