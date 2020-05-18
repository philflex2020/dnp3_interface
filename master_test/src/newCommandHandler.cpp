/*
*  newCommandHandler.cpp
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
* we'll try and make both outputs the same
* the config file will contain the choice of pub output mode and details of the DNP3 - Modbus conversion 
*
*/

#include "newCommandHandler.h"

#include "dnp3_utils.h"
#include <iostream>


using namespace opendnp3;

//newCommandHandler::newCommandHandler(const std::vector<uint8_t> iopins)
//{
//    uint16_t index = 0;

//    for(auto pin : iopins) {
//       std::cout << "                 ************" << __FUNCTION__ << " added index:" <<index <<" pin "<< (int)pin << std::endl;
//       dnp2io[index] = (int)pin;
//       ++index;
//    }
//    std::cout << "                 ************" << __FUNCTION__ << " called index:" <<index << std::endl;
    
//}
void newCommandHandler::Start()
{
  std::cout << "               ************" <<__FUNCTION__ << " called " << std::endl;
  if(cfgdb->cj)
  {
      cJSON_Delete(cfgdb->cj);
  }
  cfgdb->cj = cJSON_CreateObject();
  cfgdb->cjloaded = 0;
  
}
// Uri:     /id/MyPubs/outputs/dnp3_outstation
// ReplyTo: (null)
// Body:    {"AnalogInt16":[{"offset":8,"value":1234}],"AnalogInt32":[{"offset":0,"value":52},{"offset":2,"value":5}],"Timestamp":"05-12-2020 04:11:00.973867"}
// Met
void newCommandHandler::End()
{
  std::cout << "               ************" <<__FUNCTION__ << " called loaded = "<< cfgdb->cjloaded << std::endl;
   if(cfgdb->cjloaded) 
   {
      if(cfgdb->cj)
        {
            pubWithTimeStamp2(cfgdb->cj, cfgdb, "components");
            cJSON_Delete(cfgdb->cj);
            cfgdb->cj = NULL;
        }
        cfgdb->cjloaded = 0; 
   }
}

CommandStatus newCommandHandler::Select(const ControlRelayOutputBlock& command, uint16_t index)
{
    uint8_t io = 0;
    bool state = false;
    std::cout << "               ************" <<__FUNCTION__ << " called index:" <<index << std::endl;
    CommandStatus cs = GetPinAndState(index, command.functionCode, io, state);
    //TODO decode index
    //TODO add other command states
    if (command.functionCode == ControlCode::LATCH_ON)
    {
        cfgdbAddtoRecord(cfgdb,"CROB_SELECT","LATCH_ON", index);
    }
    else
    {
        cfgdbAddtoRecord(cfgdb,"CROB_SELECT","LATCH_OFF", index);
    }
    
    return cs;
}

CommandStatus newCommandHandler::Operate(const ControlRelayOutputBlock& command, uint16_t index, OperateType opType)
{
    uint8_t io = 0;
    bool state = false;
    std::cout << "              ************" << __FUNCTION__ << " called index:" <<index << std::endl;

    auto ret = GetPinAndState(index, command.functionCode, io, state);

    if(ret == CommandStatus::SUCCESS)
    {
         std::cout <<" operate on pin " << (int)io <<std::endl;
        //digitalWrite(gpio, state);
    }
    if (command.functionCode == ControlCode::LATCH_ON)
    {
        cfgdbAddtoRecord(cfgdb,"CROB_DIRECT","LATCH_ON", index);
    }
    else
    {
        cfgdbAddtoRecord(cfgdb,"CROB_DIRECT","LATCH_OFF", index);
    }

    return ret;
}

CommandStatus newCommandHandler::Select(const AnalogOutputInt16& command, uint16_t index)
{ 
    std::cout << "              ************ int16 " << __FUNCTION__ 
    //<< " called, code:" <<(int)code
    << " index:" <<index
    //<< " io:" <<(int)io
    << std::endl;
    cfgdbAddtoRecord(cfgdb,"AnalogInt16_SELECT",command, index);
    return CommandStatus::SUCCESS; 
}
 
CommandStatus newCommandHandler::Operate(const AnalogOutputInt16& command, uint16_t index, OperateType opType)
{ 
    std::cout << "              ************ int16 " << __FUNCTION__ 
    //<< " called, code:" <<(int)code
    << " index:" <<index
    << " value:" << (int)command.value
    << " opType:" <<(int)opType
    << std::endl;
    cfgdbAddtoRecord(cfgdb,"AnOPInt16",command, index);
    cfgdb->setDbVarIx(AnIn16, index, command.value);

    return CommandStatus::SUCCESS; 
}
CommandStatus newCommandHandler::Select(const AnalogOutputInt32& command, uint16_t index)
{ 
    std::cout << "              ************ int32 " << __FUNCTION__ 
    //<< " called, code:" <<(int)code
    << " index:" <<index
    //<< " io:" <<(int)io
    << std::endl;
    cfgdbAddtoRecord(cfgdb,"AnalogInt32_SELECT",command, index);

    return CommandStatus::SUCCESS; 
}

CommandStatus newCommandHandler::Operate(const AnalogOutputInt32& command, uint16_t index, OperateType opType)
{ 
    std::cout << "              ************ int32" << __FUNCTION__ 
    //<< " called, code:" <<(int)code
    << " index:" <<index
    << " value:" << (int)command.value
    << " opType:" <<(int)opType
    << std::endl;
    cfgdbAddtoRecord(cfgdb,"AnOPInt32", command, index);
    cfgdb->setDbVarIx(AnIn32, index, command.value);

    return CommandStatus::SUCCESS; 
}

CommandStatus newCommandHandler::Select(const AnalogOutputFloat32& command, uint16_t index)
{ 
    std::cout << "              ************ float32 " << __FUNCTION__ 
    //<< " called, code:" <<(int)code
    << " index:" <<index
    //<< " io:" <<(int)io
    << std::endl;
    cfgdbAddtoRecord(cfgdb,"AnalogFloat32_SELECT",command, index);

    return CommandStatus::SUCCESS; 
}

CommandStatus newCommandHandler::Operate(const AnalogOutputFloat32& command, uint16_t index, OperateType opType)
{ 
    std::cout << "              ************ float32" << __FUNCTION__ 
    //<< " called, code:" <<(int)code
    << " index:" <<index
    << " value:" << (int)command.value
    << " opType:" <<(int)opType
    << std::endl;
    cfgdbAddtoRecord(cfgdb,"AnOPF32",command, index);
    return CommandStatus::SUCCESS; 
}

CommandStatus newCommandHandler::GetPinAndState(uint16_t index, opendnp3::ControlCode code, uint8_t& io, bool& state)
{
    std::cout << "              ************" << __FUNCTION__ 
    << " called, code:" <<(int)code
    << " index:" <<index
    << " io:" <<(int)io
    << std::endl;
   switch(code)
    {
        case(ControlCode::LATCH_ON):
            std::cout << "              ************" << __FUNCTION__ 
            << " called, code:" <<(int)code
            << " index:" <<index
            << " io:" <<(int)io
            << " LATCH_ON" 
            << std::endl;
            state = true;
            break;

        case(ControlCode::LATCH_OFF):
            std::cout << "              ************" << __FUNCTION__ 
            << " called, code:" <<(int)code
            << " index:" <<index
            << " io:" <<(int)io
            << " LATCH_OFF" 
            << std::endl;
            state = false;
            break;

        default:
            std::cout << "              ************" << __FUNCTION__ 
            << " called, code:" <<(int)code
            << " index:" <<index
            << " io:" <<(int)io
            << " NOT_SUPPORTED" 
            << std::endl;
            
            return CommandStatus::NOT_SUPPORTED;
    }

    //auto iter = dnp2io.find(index);
    //if(iter == dnp2io.end()) {
    //        std::cout << "              ************" << __FUNCTION__ 
    //        << " called, code:" <<(int)code
    //        << " index:" <<index
    //        << " io:" <<(int)io
    //        << " INDEX NOT_SUPPORTED" 
    //        << std::endl;
 
    //    return CommandStatus::NOT_SUPPORTED;
    //}
    //io = iter->second;

    std::cout << "              ************" << __FUNCTION__ 
    << " called, code:" <<(int)code
    << " index:" <<index
    << " io:" <<(int)io
    << " INDEX FOUND" 
    << std::endl;

    return CommandStatus::SUCCESS;
}