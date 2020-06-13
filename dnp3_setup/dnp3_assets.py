#!/bin/python3
# simple python config gemerator for np3
import sys
import json

def myfun( fname):
#	with open('path_to_file/person.json') as f:
	with open(fname) as f:
  		data = json.load(f)

	# Output: {'name': 'Bob', 'languages': ['English', 'Fench']}
	print(data)
	return data


if __name__ == "__main__" :
	print ("hello")
	if len(sys.argv) > 1:
		fname = sys.argv[1]
		print(" file is :%s"% (fname))
		cj = myfun(fname)
		cja = cj["assets"]
		cjf = cja["feeders"]
		cji = cjf["asset_instances"]
		inum = len(cji)
		print( " num instances is %d" % (inum))
		for cjin in  cji:
			print("   instance id %s" %(cjin["id"]))
			for cjc in cjin["components"]:
				print ("        component_id : %s " % (cjc["component_id"]))
 

#{
#        "assets":
#        {
#                "feeders":
#                {
#                        "poi_feeder": 0,
#                        "asset_instances":
#                        [
#                                {
#                                        "id": "feed_1",
#                                        "name": "POI (SEL 651/735)",
#                                        "components":
#                                        [
#                                                {
#                                                        "component_id": "sel_651",


