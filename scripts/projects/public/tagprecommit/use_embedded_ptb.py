#!/usr/bin/env python

import sys
import os

if len(sys.argv) != 1:
    print >> sys.stderr, "Wrong number of arguments. Usage:\n" \
                         "use_embedded_ptb.py"
    sys.exit(1)

build_system = os.path.join( "c++", "src", "build-system" )

if not os.path.exists(build_system):
    print >> sys.stderr, "Can't find build-system at %s" % build_system
    sys.exit(2)

def patch(path, name):
    fName = os.path.join(build_system, name)
    if not os.path.exists(fName):
        print >> sys.stderr, "Can't find ptb_version.txt at %s" % fName
        sys.exit(3)

    f = open( fName, "wb" )
    try:
        f.write("embedded\n")
    finally:
        f.close()

patch(build_system, "ptb_version.txt")
patch(build_system, "datatool_version.txt")

sys.exit( 0 )

