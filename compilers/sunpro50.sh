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
CXXFLAGS="+w +w2 -g -D_DEBUG -DNCBI_USE_NEW_IOSTREAM"
AR="$CXX -xar -o"
RANLIB=":"
LDFLAGS="-xildoff"

PM_SRC_PATH="/net/neptune/pubmed/Cvs"
PM_LIB_PATH=""
NCBI_C_INCLUDE="/netopt/ncbi_tools/ver0.0/ncbi/include"
NCBI_C_LIB_PATH="/netopt/ncbi_tools/ver0.0/ncbi/altlib"

sh -xv configure --exec_prefix=.
