#ifndef DNP3_NEWCOMMANDHANDLER_H
#define DNP3_NEWCOMMANDHANDLER_H

#include <opendnp3/outstation/ICommandHandler.h>

#include "dnp3_utils.h"

//#include <vector>
//#include <map>

class newCommandHandler final : public opendnp3::ICommandHandler
{

public:
    newCommandHandler(sysCfg* myDB);

    static std::shared_ptr<ICommandHandler> Create(sysCfg* db)
    {
        return std::make_shared<newCommandHandler>(db);
    }

    void Start() override {}
    void End() override {}

    opendnp3::CommandStatus Select(const opendnp3::ControlRelayOutputBlock& command, uint16_t index) override;

    opendnp3::CommandStatus Operate(const opendnp3::ControlRelayOutputBlock& command, uint16_t index, opendnp3::OperateType opType) override;

    opendnp3::CommandStatus Select(const opendnp3::AnalogOutputInt16& command, uint16_t index) override;
    //{ return opendnp3::CommandStatus::NOT_SUPPORTED; }

    opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputInt16& command, uint16_t index, opendnp3::OperateType opType) override;
    //{ return opendnp3::CommandStatus::NOT_SUPPORTED; }

    opendnp3::CommandStatus Select(const opendnp3::AnalogOutputInt32& command, uint16_t index) override
    { return opendnp3::CommandStatus::NOT_SUPPORTED; }

    opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputInt32& command, uint16_t index, opendnp3::OperateType opType) override
    { return opendnp3::CommandStatus::NOT_SUPPORTED; }

    opendnp3::CommandStatus Select(const opendnp3::AnalogOutputFloat32& command, uint16_t index) override
    { return opendnp3::CommandStatus::NOT_SUPPORTED; }

    opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputFloat32& command, uint16_t index, opendnp3::OperateType opType)
    { return opendnp3::CommandStatus::NOT_SUPPORTED; }

    opendnp3::CommandStatus Select(const opendnp3::AnalogOutputDouble64& command, uint16_t index) override
    { return opendnp3::CommandStatus::NOT_SUPPORTED; }

    opendnp3::CommandStatus Operate(const opendnp3::AnalogOutputDouble64& command, uint16_t index, opendnp3::OperateType opType)
    { return opendnp3::CommandStatus::NOT_SUPPORTED; }

private:

    opendnp3::CommandStatus GetPinAndState(uint16_t index, opendnp3::ControlCode code, uint8_t& gpio, bool& state);

    sysCfg* cfgdb;
    //std::map<uint16_t, uint8_t> dnp2io;
};

#endif //DNP3_NEWCOMMANDHANDLER_H
