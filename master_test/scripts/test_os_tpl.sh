#!/bin/sh
# this tests a signed 32bit signed intger from outstation to master
# use hybridos_storage_master/outstation  config
#VAR=remote_export_target_kW_cmd
VAR=$1
VAL=$2

#normal within bounds

echo " **** sending val [$VAL]"

fims_send -m set -u /interfaces/hybridos/$VAR -r /me "{\"value\":$VAL}"

echo "value from master" 
fims_send -m get -u /components/hybridos_m/$VAR -r /me 
echo "value from outstation" 
fims_send -m get -u /interfaces/hybridos/$VAR -r /me 
echo



