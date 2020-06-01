#!/bin/sh
# simple tests for master

/usr/local/bin/fims/fims_send -m get -r /me -u /components/dnp3_master
sleep 4
 
/usr/local/bin/fims/fims_send -m get -r /me -u /components/dnp3_master/voltage_regulator 
sleep 4
/usr/local/bin/fims/fims_send -m set -r /me -u /components/dnp3_master '{"testCROB_4":"LATCH_OFF"}'
sleep 4
/usr/local/bin/fims/fims_send -m set -r /me -u /components/dnp3_master/testCROB_4 '{"testCROB_4":"LATCH_ON"}'
sleep 4
/usr/local/bin/fims/fims_send -m set -r /me -u /components/dnp3_master/testCROB_4 '{"testCROB_4":"PULSE_ON"}'
sleep 4
/usr/local/bin/fims/fims_send -m set -r /me -u /components/dnp3_master/testCROB_4 '{"testCROB_4":"PULSE_OFF"}'
sleep 4
#/usr/local/bin/fims/fims_send -m set -r /me -u /components/dnp3_master/testCROB_4 'PULSE_OFF'
#sleep 4
#/usr/local/bin/fims/fims_send -m set -r /me -u /components/dnp3_master/testCROB_4 'PULSE_ON'
#sleep 4
