
#include <iostream>
#include <string>
#include <thread>
#include <stdio.h>
#include <stdint.h>

#include <fims/libfims.h>
#include <string.h>
#include <fims/fps_utils.h>
#include <cjson/cJSON.h>


#include <opendnp3/LogLevels.h>
#include <opendnp3/outstation/IUpdateHandler.h>
#include <opendnp3/outstation/SimpleCommandHandler.h>

#include <asiodnp3/ConsoleLogger.h>
#include <asiodnp3/DNP3Manager.h>
#include <asiodnp3/PrintingChannelListener.h>
#include <asiodnp3/PrintingSOEHandler.h>
#include <asiodnp3/UpdateBuilder.h>

using namespace std;
using namespace opendnp3;
using namespace openpal;
using namespace asiopal;
using namespace asiodnp3;

fims *p_fims;

void ConfigureDatabase(DatabaseConfig& config)
{
    // example of configuring analog index 0 for Class2 with floating point variations by default
    config.analog[0].clazz = PointClass::Class2;
    config.analog[0].svariation = StaticAnalogVariation::Group30Var5;
    config.analog[0].evariation = EventAnalogVariation::Group32Var7;
}

struct State
{
    uint32_t count = 0;
    double value = 0;
    bool binary = false;
    DoubleBit dbit = DoubleBit::DETERMINED_OFF;
    uint8_t octetStringValue = 1;
};

std::shared_ptr<asiodnp3::IOutstation> outstation_init(asiodnp3::DNP3Manager &manager) {
    // Specify what log levels to use. NORMAL is warning and above
    // You can add all the comms logging by uncommenting below.
    const uint32_t FILTERS = levels::NORMAL | levels::ALL_COMMS;

    // Create a TCP server (listener)
    auto channel = manager.AddTCPServer("server", FILTERS, ServerAcceptMode::CloseExisting, "127.0.0.1", 20001,
                                        PrintingChannelListener::Create());

    // The main object for a outstation. The defaults are useable,
    // but understanding the options are important.
    OutstationStackConfig config(DatabaseSizes::AllTypes(10));

    // Specify the maximum size of the event buffers
    config.outstation.eventBufferConfig = EventBufferConfig::AllTypes(10);

    // you can override an default outstation parameters here
    // in this example, we've enabled the oustation to use unsolicted reporting
    // if the master enables it
    config.outstation.params.allowUnsolicited = true;

    // You can override the default link layer settings here
    // in this example we've changed the default link layer addressing
    config.link.LocalAddr = 1;
    config.link.RemoteAddr = 0;

    config.link.KeepAliveTimeout = openpal::TimeDuration::Max();

    // You can optionally change the default reporting variations or class assignment prior to enabling the outstation
    ConfigureDatabase(config.dbConfig);

    // Create a new outstation with a log level, command handler, and
    // config info this	returns a thread-safe interface used for
    // updating the outstation's database.
    auto outstation = channel->AddOutstation("outstation", SuccessCommandHandler::Create(),
                                             DefaultOutstationApplication::Create(), config);
    printf("server channel created\n");

    // Enable the outstation and start communications
    std::cout << "Enabling" << std::endl;   
    outstation->Enable();
    std::cout << "outstation channel enabled" << std::endl;
    return outstation;
}

int main(int argc, char* argv[])
{
    // char *subscriptions[1];
    // int rtn_val = 0;
    // int num_subs = 1;

    char sub[] = "/test";
    char* subs = sub;
    bool publish_only = false;
    bool running = true;

    p_fims = new fims();

    // This is the main point of interaction with the stack
    // Allocate a single thread to the pool since this is a single outstation
    // Must be in main scope
    DNP3Manager manager(1, ConsoleLogger::Create());
    auto outstation = outstation_init(manager);
    printf("outstation started\n");

    if(p_fims->Connect((char *)"fims_listen") == false)
    {
        printf("Connect failed.\n");
        p_fims->Close();
        return 1;
    }

    if(p_fims->Subscribe((const char**)&subs, 1, (bool *)&publish_only) == false)
    {
        printf("Subscription failed.\n");
        p_fims->Close();
        return 1;
    }

    bool bin = false;

    while(running && p_fims->Connected())
    {
        fims_message* msg = p_fims->Receive();
        if(msg != NULL)
        {
            cJSON* body_JSON = cJSON_Parse(msg->body);
            if (body_JSON == NULL)
            {
                FPS_ERROR_PRINT("fims message body is NULL or incorrectly formatted: (%s) \n", msg->body);
                return 1;
            }
            cJSON* body_value = cJSON_GetObjectItem(body_JSON, "value");
            if (body_value == NULL)
            {
                FPS_ERROR_PRINT("fims message body value key not found \n");
                return 1;
            }
            bin = !bin;
            printf("bodyval: %f\n", body_value->valuedouble);
            UpdateBuilder builder;
            builder.Update(Analog(body_value->valuedouble), 0);
            builder.Update(Binary(bin), 0);
            outstation->Apply(builder.Build());
        }
    }
}
