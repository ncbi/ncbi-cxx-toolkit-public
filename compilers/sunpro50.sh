#!/bin/sh
#
# $Id$
# Set tools and flags specific for SunPro 5.0 compilers, then run "configure"
#
# By Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#############################################################################

set -a

CPATH="/netopt/SUNWspro6/bin"

CC="$CPATH/cc"
CFLAGS="-Xc -g -D_DEBUG"
CXX="$CPATH/CC"
CXXFLAGS="+w +w2 -g -D_DEBUG"
AR="$CXX -xar -o"
RANLIB=":"
LDFLAGS="-xildoff"

sh configure
