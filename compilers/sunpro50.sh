#!/bin/sh
#
# $Id$
# Set tools and flags specific for SunPro 5.0 compilers, then run "configure"
#
# By Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#############################################################################

USR_BUILD_DIR="$1"
DEF_BUILD_DIR="."

set -a

CPATH="/netopt/SUNWspro6/bin"

CC="$CPATH/cc"
CFLAGS="-Xc -g -D_DEBUG"
CXX="$CPATH/CC"
CXXFLAGS="+w +w2 -g -D_DEBUG -DNCBI_USE_NEW_IOSTREAM"
AR="$CXX -xar -o"
RANLIB=":"
LDFLAGS="-xildoff -g"

PM_PATH="/net/neptune/pubmed1/Cvs5"
NCBI_C_INCLUDE="$NCBI/ver0.0/ncbi/include"
NCBI_C_LIB="$NCBI/ver0.0/ncbi/altlib"

KeepStateTarget=".KEEP_STATE:"

sh configure --exec_prefix=${USR_BUILD_DIR:=$DEF_BUILD_DIR}
