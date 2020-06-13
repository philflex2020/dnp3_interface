#!/bin/python3
# simple python config gemerator for dnp3
# used data from fims_listen
import sys
import json

uris = {}

def tfile(fname):
	global uris
	newuri = False
	uri = ""
	muri = ""
	for f in open(fname):
		if newuri == True:
			if f[:6] == "Body: ":
				newuri = False
				uris[muri]=json.loads(f[6:-1])
				#print(" Body Json found [%s] " % (json.dumps(uris[uri])))
				cj = uris[muri]
				match = "/components/"
				if muri.find(match) == 0:
					for k,v in cj.items():
						print("                      [%s] -> " % (k), end='')
						print(v)

				else:
					for k,v in cj.items():
						if k == "name":
							print("    Name -> val [%s] " % (v))
						else:
							print("                      [%s] " % (k))


		if f[:5] == "Uri: ":
			uri = f[9:-1]
			if uri not in uris:
				newuri = True
				muri = f[9:-1]
				print("Uri: [%s] " %(muri))
				uris[muri] = muri

def myfun( fname):
#	with open('path_to_file/person.json') as f:
	with open(fname) as f:
  		data = json.load(f)

	# Output: {'name': 'Bob', 'languages': ['English', 'Fench']}
	print(data)
	return data

def jfile(fname):
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


if __name__ == "__main__" :
	print ("hello")
	if len(sys.argv) > 1:
		fname = sys.argv[1]
		print(" file is :%s"% (fname))
		tfile(fname)
 

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


