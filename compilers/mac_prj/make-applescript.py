#!/bin/env python
#
# $Id$
#
# Author: Paul Thiessen
#
# This is a Python script I use to output part of the makelibs.met
# applescript that populates and builds the Mac CodeWarrior projects.
#

import string, glob

root = "/net/scarecrow/export/home/thiessen/Programs/c++/src/"
exclude = [ "__.cpp" ]
objects = [ "access", "biblio", "cdd", "featdef", "general", "medlars",
	"medline", "mmdb1", "mmdb2", "mmdb3", "ncbimime", "objprt", "proj",
	"pub", "pubmed", "seq", "seqalign", "seqblock", "seqcode", "seqfeat",
	"seqloc", "seqres", "seqset", "submit" ]
quickobjects = 1

def proj(dir, name, macpath):
	global root, exclude
	files = []

	if quickobjects and dir[:7] == "objects":
		files.append(dir[string.rindex(dir,"/")+1:] + "__")
		files.append(dir[string.rindex(dir,"/")+1:] + "___")
	else:
		tmpfiles = glob.glob(root + dir + "/*.cpp")
		for i in tmpfiles:
			for j in exclude:
				if i[-len(j):] == j: break
				else:
					files.append(i[string.rindex(i,"/")+1:-4])
					
	files.sort()
	print "my DoCreateDefaultProject(\"" + name + "\", \"" + macpath + "\", Â\n{" ,
	for i in range(len(files)):
		if i > 0: print "," ,
		if i > 0 and i%4 == 0: print " Â"
		print "\"" + files[i] + "\"" ,
	print "})\n"


proj("corelib", "corelib", "corelib")
proj("serial", "serial", "serial")
proj("html", "html", "html")
proj("connect", "connect", "connect")
for i in objects:
	proj("objects/" + i, "object_" + i, "objects:" + i)
for i in objects:
	print "my DoCompileLibrary(\"object_" + i + "\")"
