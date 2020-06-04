#/bin/sh


/usr/local/bin/fims/fims_echo -u /queryx/home/mystuff/uri -b '{"testOP_32_1":1234,"testOP_32_22":3222}' &
/usr/local/bin/fims/fims_echo -u /queryx/home/athings/uri -b '{"testOP_16_8":1608}' &
/usr/local/bin/fims/fims_echo -u /getUris/components/sel_735 -b '{
"wh_received":23,
"wh_delivered":24,
"voltage_ca":440,
"voltage_bc":430,
"voltage_ab":420,
"varh_received":1,
"varh_delivered":23,
"thd_v_l3":12,
"thd_v_l2":34,
"thd_v_l1":56,
"thd_i_l3":78,
"thd_i_l2":90,
"thd_i_l1":12,
"reactive_power":4567,
"power_factor":23,
"frequency":440,
"current_c":3000,
"current_b":3500,
"current_a":3600,
"apparent_power":23,
"active_power":434
                 }' &



