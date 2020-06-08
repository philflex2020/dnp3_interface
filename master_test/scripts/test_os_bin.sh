#!/bin/sh
# this tests a signed 32bit signed intger from master to outstation
# use hybridos_storage_master/outstation  config
VAR=present_enable_flag
sh scripts/test_os_tpl.sh $VAR 1
sh scripts/test_os_tpl.sh $VAR 0
#sh scripts/test_os_tpl.sh $VAR 1
#sh scripts/test_os_tpl.sh $VAR 1
sh scripts/test_os_tpl.sh $VAR 256
sh scripts/test_os_tpl.sh $VAR 65535
sh scripts/test_os_tpl.sh $VAR "-65535"
exit 0


#normal within bounds
echo ' normal +1 -1'

VAL=1
fims_send -m set -u /components/hybridos_m/$VAR -r /me "{\"value\":$VAL}"
#{"remote_export_target_kW_cmd":{"value":1}}

echo "reply from master" 
fims_send -m get -u /components/hybridos_m/$VAR -r /me 
echo "reply from outstation" 
fims_send -m get -u /interfaces/hybridos/$VAR -r /me 
echo

fims_send -m set -u /components/hybridos_m/$VAR -r /me "{\"value\":-$VAL}"
#{"remote_export_target_kW_cmd":{"value":-1}}
echo "reply from master" 
fims_send -m get -u /components/hybridos_m/$VAR -r /me 
echo "reply from outstation" 
fims_send -m get -u /interfaces/hybridos/$VAR -r /me 
echo

echo 'try +/- 65535'
VAL=65535

fims_send -m set -u /components/hybridos_m/$VAR -r /me "{\"value\":$VAL}"
#{"remote_export_target_kW_cmd":{"value":65535}}
echo "reply from master" 
fims_send -m get -u /components/hybridos_m/$VAR -r /me 
fims_send -m get -u /interfaces/hybridos/$VAR -r /me 

fims_send -m set -u /components/hybridos_m/$VAR -r /me "{\"value\":-$VAL}"
#{"remote_export_target_kW_cmd":{"value":-65535}}
echo "reply from master" 
fims_send -m get -u /components/hybridos_m/$VAR -r /me 
echo "reply from outstation" 
fims_send -m get -u /interfaces/hybridos/$VAR -r /me 
echo

#move out to 2147483647
echo 'try +/- 214748347'
VAL=2147483647

fims_send -m set -u /components/hybridos_m/$VAR -r /me "{\"value\":$VAL}"
#{"remote_export_target_kW_cmd":{"value":214748647}}
echo "reply from master" 
fims_send -m get -u /components/hybridos_m/$VAR -r /me 
echo "reply from outstation" 
fims_send -m get -u /interfaces/hybridos/$VAR -r /me 
#{"remote_export_target_kW_cmd":{"value":214748647}}

fims_send -m set -u /components/hybridos_m/$VAR -r /me "{\"value\":-$VAL}"
#{"remote_export_target_kW_cmd":{"value":-214748647}}
echo "reply from master" 
fims_send -m get -u /components/hybridos_m/$VAR -r /me 
echo "reply from outstation" 
fims_send -m get -u /interfaces/hybridos/$VAR -r /me 
#{"remote_export_target_kW_cmd":{"value":-214748647}}
echo

#move out to 3147483647
echo ' try +/- 3147483647'
VAL=3147483647

fims_send -m set -u /components/hybridos_m/$VAR -r /me "{\"value\":$VAL}"
#{"remote_export_target_kW_cmd":{"value":214748647}}
echo "reply from master" 
fims_send -m get -u /components/hybridos_m/$VAR -r /me 
echo "reply from outstation" 
fims_send -m get -u /interfaces/hybridos/$VAR -r /me 
#{"remote_export_target_kW_cmd":{"value":214748647}}

fims_send -m set -u /components/hybridos_m/$VAR -r /me "{\"value\":-$VAL}"
#{"remote_export_target_kW_cmd":{"value":-214748647}}
echo "reply from master" 
fims_send -m get -u /components/hybridos_m/$VAR -r /me 
echo "reply from outstation" 
fims_send -m get -u /interfaces/hybridos/$VAR -r /me 
#{"remote_export_target_kW_cmd":{"value":-214748647}}



