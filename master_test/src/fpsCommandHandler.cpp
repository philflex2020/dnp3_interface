/*
*  fpsCommandHandler.cpp
* author :pwilshire
*  11 May, 2020
*
* This runs in the outstation and will handle the control requests from the master.
* There are two variants of this
* one will be the customer facing outstation in the Fleet Manage.
* the other will be the site outstations in the FlexGen satellite sites.
* the FM outstation will provide a pub intensed for the FM Master stations.
* this pub will contain all the details of the command from the Customer Master station.
* The second output will be a pub intended for the MODBUS clients on the site systems.
* communcations with the MODBUS clients are via set /get 
*  /components/sys.cfgname [{"name":"value"},{"name":"value"},...]
*  /components/sys.cfgname/item {"value":"value"}
* make both outputs the same
* the config file will contain the choice of pub output mode and details of the DNP3 - Modbus conversion 
*
*/

#include "fpsCommandHandler.h"

#include "dnp3_utils.h"
#include <iostream>


using namespace opendnp3;

void fpsCommandHandler::Start()
{
  if(sysdb->cj)
  {
      cJSON_Delete(sysdb->cj);
  }
  sysdb->cj = cJSON_CreateObject();
  sysdb->cjloaded = 0;
  
}
// Uri:     /components/dnp3_outstation
// ReplyTo: (null)
// Body:    {"AnalogInt16":[{"offset":8,"value":1234}],"AnalogInt32":[{"offset":0,"value":52},{"offset":2,"value":5}],"Timestamp":"05-12-2020 04:11:00.973867"}
// Met
void fpsCommandHandler::End()
{
    //std::cout << "               ************" <<__FUNCTION__ << " called loaded = "<< sysdb->cjloaded << std::endl;
    if(sysdb->cjloaded) 
    {
      if(sysdb->cj)
        {
            // make sure we want to do this. Look at modbus
            pubWithTimeStamp(sysdb->cj, sysdb, "components");
            cJSON_Delete(sysdb->cj);
            sysdb->cj = NULL;
        }
        sysdb->cjloaded = 0; 
   }
}

CommandStatus fpsCommandHandler::Select(const ControlRelayOutputBlock& command, uint16_t index)
{
    const char* cmd = ControlCodeToString(command.functionCode);

    sysdbAddtoRecord(sysdb,"CROB_SELECT", cmd, index);
    return CommandStatus::SUCCESS; 
}

CommandStatus fpsCommandHandler::Operate(const ControlRelayOutputBlock& command, uint16_t index, OperateType opType)
{
    FPS_DEBUG_PRINT("                     ************ %s  CROB called, index: %d \n", __FUNCTION__, (int) index );
    const char* cmd = ControlCodeToString(command.functionCode);
    sysdbAddtoRecord(sysdb, "CROB_DIRECT", cmd, index);
    sysdb->setDbVarIx(Type_Crob, index, ControlCodeToType(command.functionCode));
    return CommandStatus::SUCCESS; 
}

CommandStatus fpsCommandHandler::Select(const AnalogOutputInt16& command, uint16_t index)
{ 
    sysdbAddtoRecord(sysdb, "AnalogInt16_SELECT", command, index);
    return CommandStatus::SUCCESS; 
}
 
CommandStatus fpsCommandHandler::Operate(const AnalogOutputInt16& command, uint16_t index, OperateType opType)
{ 
    FPS_DEBUG_PRINT("                     ************ %s  AnInt16 called, index: %d \n", __FUNCTION__, (int) index );
    sysdbAddtoRecord(sysdb,"AnOPInt16", command, index);
    sysdb->setDbVarIx(AnIn16, index, command.value);
    return CommandStatus::SUCCESS; 
}

CommandStatus fpsCommandHandler::Select(const AnalogOutputInt32& command, uint16_t index)
{ 
    sysdbAddtoRecord(sysdb, "AnalogInt32_SELECT", command, index);
    return CommandStatus::SUCCESS; 
}

CommandStatus fpsCommandHandler::Operate(const AnalogOutputInt32& command, uint16_t index, OperateType opType)
{ 
    FPS_DEBUG_PRINT("                     ************ %s  AnInt32 called, index: %d \n", __FUNCTION__, (int) index );
    sysdbAddtoRecord(sysdb, "AnOPInt32", command, index);
    sysdb->setDbVarIx(AnIn32, index, command.value);
    return CommandStatus::SUCCESS; 
}

CommandStatus fpsCommandHandler::Select(const AnalogOutputFloat32& command, uint16_t index)
{ 
    sysdbAddtoRecord(sysdb, "AnalogFloat32_SELECT", command, index);
    return CommandStatus::SUCCESS; 
}

CommandStatus fpsCommandHandler::Operate(const AnalogOutputFloat32& command, uint16_t index, OperateType opType)
{ 
    FPS_DEBUG_PRINT("                     ************ %s  AnF32 called, index: %d \n", __FUNCTION__, (int) index );
    sysdbAddtoRecord(sysdb, "AnOPF32", command, index);
    sysdb->setDbVarIx(AnF32, index, command.value);    
    return CommandStatus::SUCCESS; 
}