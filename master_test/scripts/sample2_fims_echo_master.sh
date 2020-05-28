#/bin/sh

/usr/local/bin/fims/fims_echo -u /queryx/home/mystuff/uri -b '{"testOP_32_1":1234,"testOP_32_22":3222}' &
/usr/local/bin/fims/fims_echo -u /queryx/home/athings/uri -b '{"testOP_16_8":1608}' &
/usr/local/bin/fims/fims_echo -u /queryx/dnp3_outstation -b '{
             "testCROB_4":"LATCH_OFF",
          "line_flows_mw":23456,
          "line_flows_mv":480,
                "voltage":48,
"plant_auxiliary_load_mw":1134,
 "plant_auxiliary_load_mv":4800,
     "voltage_regulator":1,
"power_system_stabilizer":0,
  "gen_resource_switches":1,
"gen_resource_breaker_status":12,
  "gen_virtual_breaker_status":23
                 }' &



