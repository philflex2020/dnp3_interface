#!/bin/sh
# this tests a signed 32bit signed intger from master to outstation
# use hybridos_storage_master/outstation  config
VAR=remote_export_target_kW_cmd

#normal within bounds
echo ' normal +1 -1'

VAL=1
fims_send -m set -u /components/hybridos_m/$VAR -r /me "{\"value\":$VAL}"
#{"remote_export_target_kW_cmd":{"value":1}}
fims_send -m get -u /components/hybridos_m/$VAR -r /me 
fims_send -m set -u /components/hybridos_m/$VAR -r /me "{\"value\":-$VAL}"
#{"remote_export_target_kW_cmd":{"value":-1}}
fims_send -m get -u /components/hybridos_m/$VAR -r /me 

echo 'try +/- 65535'
VAL=65535

fims_send -m set -u /components/hybridos_m/$VAR -r /me "{\"value\":$VAL}"
#{"remote_export_target_kW_cmd":{"value":65535}}
fims_send -m get -u /components/hybridos_m/$VAR-r /me 

fims_send -m set -u /components/hybridos_m/$VAR -r /me "{\"value\":-$VAL}"
#{"remote_export_target_kW_cmd":{"value":-65535}}
fims_send -m get -u /components/hybridos_m/$VAR -r /me 

#move out to 2147483647
echo 'try +/- 214748347'
VAL=2147483647

fims_send -m set -u /components/hybridos_m/$VAR -r /me "{\"value\":$VAL}"
#{"remote_export_target_kW_cmd":{"value":214748647}}
fims_send -m get -u /components/hybridos/$VAR -r /me 
#{"remote_export_target_kW_cmd":{"value":214748647}}

fims_send -m set -u /components/hybridos_m/$VAR -r /me "{\"value\":-$VAL}"
#{"remote_export_target_kW_cmd":{"value":-214748647}}
fims_send -m get -u /components/hybridos/$VAR -r /me 
#{"remote_export_target_kW_cmd":{"value":-214748647}}

#move out to 3147483647
echo ' try +/- 3147483647'
VAL=3147483647

fims_send -m set -u /components/hybridos_m/$VAR -r /me "{\"value\":$VAL}"
#{"remote_export_target_kW_cmd":{"value":214748647}}
fims_send -m get -u /components/hybridos/$VAR -r /me 
#{"remote_export_target_kW_cmd":{"value":214748647}}
fims_send -m set -u /components/hybridos_m/$VAR -r /me "{\"value\":-$VAL}"
#{"remote_export_target_kW_cmd":{"value":-214748647}}
fims_send -m get -u /components/hybridos/$VAR -r /me 
#{"remote_export_target_kW_cmd":{"value":-214748647}}



