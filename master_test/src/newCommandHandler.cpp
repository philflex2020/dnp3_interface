#include "newCommandHandler.h"

#include "dnp3_utils.h"
#include <iostream>

//#include <wiringPi.h>

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

CommandStatus newCommandHandler::Select(const ControlRelayOutputBlock& command, uint16_t index)
{
    uint8_t io = 0;
    bool state = false;
    std::cout << "               ************" <<__FUNCTION__ << " called index:" <<index << std::endl;
    return GetPinAndState(index, command.functionCode, io, state);
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

    return ret;
}

CommandStatus newCommandHandler::Select(const AnalogOutputInt16& command, uint16_t index)
{ 
    std::cout << "              ************ int16 " << __FUNCTION__ 
    //<< " called, code:" <<(int)code
    << " index:" <<index
    //<< " io:" <<(int)io
    << std::endl;

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
    return CommandStatus::SUCCESS; 
}
CommandStatus newCommandHandler::Select(const AnalogOutputInt32& command, uint16_t index)
{ 
    std::cout << "              ************ int32 " << __FUNCTION__ 
    //<< " called, code:" <<(int)code
    << " index:" <<index
    //<< " io:" <<(int)io
    << std::endl;

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
    return CommandStatus::SUCCESS; 
}

CommandStatus newCommandHandler::Select(const AnalogOutputFloat32& command, uint16_t index)
{ 
    std::cout << "              ************ float32 " << __FUNCTION__ 
    //<< " called, code:" <<(int)code
    << " index:" <<index
    //<< " io:" <<(int)io
    << std::endl;

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