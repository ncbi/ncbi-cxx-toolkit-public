#!/usr/bin/env python

import sys, os.path

if len(sys.argv) != 1:
    print >> sys.stderr, "Wrong number of arguments. Usage:\n" \
                         "use_embedded_ptb.py"
    sys.exit(1)

build_system = os.path.join( "c++", "src", "build-system" )

if not os.path.exists(build_system):
    print >> sys.stderr, "Can't find build-system at %s" % build_system
    sys.exit(2)

def patch(path, name):
    fname = os.path.join(build_system, name)
    if not os.path.exists(fname):
        print >> sys.stderr, "Can't find ptb_version.txt at %s" % fname
        sys.exit(3)

    f = open( fname, "wb" )
    try:
        f.write("embedded\n")
    finally:
        f.close()

patch(build_system, "ptb_version.txt")
patch(build_system, "datatool_version.txt")

sys.exit( 0 )

