#!/usr/bin/env python

import sys, os.path

if len( sys.argv ) != 3:
    print >> sys.stderr, "Wrong number of arguments. Usage:\n" \
                         "create_version.py local version"
    sys.exit( 1 )

local = sys.argv[ 1 ].upper() == "TRUE"
version = sys.argv[ 2 ]

if local:
    sys.exit( 0 )

fName = os.path.sep.join( [ "c++", "src", "internal", "cppcore", "test_stat_ext", "cgi", "xsl", "package_version.xsl" ] )
f = open( fName, "w" )
f.write(
"""
<?xml version="1.0" ?>

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

  <xsl:template name="package_version">""" + version + """</xsl:template>

</xsl:stylesheet>
""" )
f.close()

sys.exit( 0 )

