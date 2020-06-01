#!/bin/sh
#readback test

#set Voltage Set point to 550 on master
fims_send -m set -u /components/dnp3_master/voltageSP 550 -r /me
#expect {"voltageSP":550}

# get Voltage Setpoint from outstation
fims_send -m get -u /components/dnp3_outstation/voltageSP -r /me
#expect {"voltageSP":550}

#update Voltage Setpoint on outstation
fims_send -m set -u /components/dnp3_outstation/voltageSP 500 -r /me
#expect {"voltageSP":500}

#check orig Voltage Setpoint on master
fims_send -m get -u /components/dnp3_master/voltageSP -r /me
#expect {"voltageSP":550}

#check readback Voltage Setpoint on master note the "_"
fims_send -m get -u /components/dnp3_master/_voltageSP -r /me
#expect {"voltageSP":500}



