#!/usr/bin/env python

import sys, os.path

if len( sys.argv ) != 2:
    print >> sys.stderr, "Wrong number of arguments. Usage:\n" \
                         "create_version.py version"
    sys.exit( 1 )

version = sys.argv[ 1 ]

fName = os.path.sep.join( [ "scripts", "common", "lib",
                            "python", "grid-python", "__init__.py" ] )
f = open( fName, "r" )
content = f.read()
f.close()


content = content.replace( "return 'unknown'", "return '" + version + "'" )

f = open( fName, "w" )
f.write( content )
f.close()

sys.exit( 0 )

