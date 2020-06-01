#!/bin/sh
/usr/local/bin/fims/fims_send -m pub -u /components/dnp3_outstation/voltage_regulator 'false'
sleep 4
/usr/local/bin/fims/fims_send -m pub -u /components/dnp3_outstation/voltage_regulator 'true'
sleep 4
/usr/local/bin/fims/fims_send -m pub -u /components/dnp3_outstation/voltage_regulator 1 
sleep 4
/usr/local/bin/fims/fims_send -m pub -u /components/dnp3_outstation/voltage_regulator 0 
sleep 4
/usr/local/bin/fims/fims_send -m pub -u /components/dnp3_outstation/voltage_regulator '{"value":true}'
sleep 4
/usr/local/bin/fims/fims_send -m pub -u /components/dnp3_outstation/voltage_regulator '{"value":false}'
sleep 4
/usr/local/bin/fims/fims_send -m pub -u /components/dnp3_outstation/voltage_regulator '{"value":1}'
sleep 4
/usr/local/bin/fims/fims_send -m pub -u /components/dnp3_outstation/voltage_regulator '{"value":0}'
sleep 4
/usr/local/bin/fims/fims_send -m pub -u /components/dnp3_outstation '{"voltage_regulator":{"value":true}}'
sleep 4
/usr/local/bin/fims/fims_send -m pub -u /components/dnp3_outstation '{"voltage_regulator":{"value":false}}'
sleep 4
/usr/local/bin/fims/fims_send -m pub -u /components/dnp3_outstation '{"voltage_regulator":{"value":1}}'
sleep 4
/usr/local/bin/fims/fims_send -m pub -u /components/dnp3_outstation '{"voltage_regulator":{"value":0}}'
sleep 4


/usr/local/bin/fims/fims_send -m pub -r /me -u /components/dnp3_outstation/voltage_regulator 'false'
sleep 4
/usr/local/bin/fims/fims_send -m pub -r /me -u /components/dnp3_outstation/voltage_regulator 'true'
sleep 4
/usr/local/bin/fims/fims_send -m pub -r /me -u /components/dnp3_outstation/voltage_regulator 1 
sleep 4
/usr/local/bin/fims/fims_send -m pub -r /me -u /components/dnp3_outstation/voltage_regulator 0 
sleep 4
/usr/local/bin/fims/fims_send -m pub -r /me -u /components/dnp3_outstation/voltage_regulator '{"value":true}'
sleep 4
/usr/local/bin/fims/fims_send -m pub -r /me -u /components/dnp3_outstation/voltage_regulator '{"value":false}'
sleep 4
/usr/local/bin/fims/fims_send -m pub -r /me -u /components/dnp3_outstation/voltage_regulator '{"value":1}'
sleep 4
/usr/local/bin/fims/fims_send -m pub -r /me -u /components/dnp3_outstation/voltage_regulator '{"value":0}'
sleep 4
/usr/local/bin/fims/fims_send -m pub -r /me -u /components/dnp3_outstation '{"voltage_regulator":{"value":true}}'
sleep 4
/usr/local/bin/fims/fims_send -m pub -r /me -u /components/dnp3_outstation '{"voltage_regulator":{"value":false}}'
sleep 4
/usr/local/bin/fims/fims_send -m pub -r /me -u /components/dnp3_outstation '{"voltage_regulator":{"value":1}}'
sleep 4
/usr/local/bin/fims/fims_send -m pub -u /components/dnp3_outstation '{"voltage_regulator":{"value":0}}'
sleep 4

