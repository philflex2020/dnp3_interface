#!/bin/sh
#linkback test  

#set Voltage Set point to 550 on master
fims_send -m set -u /components/dnp3_master/voltageSP 550 -r /me
#expect {"voltageSP":550}
# this will result in a set /site/uri/voltageSP 550 on the outstation.
#the /set/outstation will respond with a /set/outstation/site_voltageSP 450


#update Voltage Setpoint on outstation
fims_send -m set -u /components/dnp3_outstation/site_voltageSP 450 -r /me
#expect {"site_voltageSP":450}
# the linkback causes the incoming site_voltageSP on the master from the outstation 
# to also set the value of voltageSP on the master. 
#check  Voltage Setpoint on master
fims_send -m get -u /components/dnp3_master/voltageSP -r /me
#expect {"voltageSP":450}

#But Note if the master does a "set" to "voltageSP" on the outstation 
#   then also does a outstation scan before the /site/outstation updates the value of site_voltageSP
# we will ge the old value of site_voltageSP when getting the value of voltageSP on the master    

#here is the out of syc sequece.

fims_send -m set -u /components/dnp3_master/voltageSP 550 -r /me
#master runs scan of outstation.
# gets old value of site_voltageSP ( say 400)
fims_send -m get -u /components/dnp3_master/voltageSP -r /me
{"voltageSP 400"}

# outstation recieves vale from master and sends set /site/ update
fims_send -m set -u /site/voltageSP  550

#outstation then issues a pub to site_voltageSP to update master
fims_send -m pub -u /site/site_voltageSP 450

#once the master receives the message from he outstation it will update voltageSP due to the incoming site_voltageSP message
# now this works
fims_send -m get -u /components/dnp3_master/voltageSP -r /me
{"voltageSP 450"}



