#!/bin/sh
#
# $Id$
# Set tools and flags specific for SunPro 5.0 compilers, then run "configure"
#
# By Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#############################################################################

USR_BUILD_DIR="$1"
DEF_BUILD_DIR="."

USR_OPT_FLAGS="$2"
DEF_OPT_FLAGS="-g -D_DEBUG"

set -a

CPATH="/netopt/SUNWspro6/bin"

CC="$CPATH/cc"
CFLAGS="-Xc ${USR_OPT_FLAGS:=$DEF_OPT_FLAGS}"
CXX="$CPATH/CC"
CXXFLAGS="+w +w2 ${USR_OPT_FLAGS:=$DEF_OPT_FLAGS} -DNCBI_USE_NEW_IOSTREAM -DNCBI_USE_NEW_HEADERS -DFAST_CGI"
AR="$CXX -xar -o"
RANLIB=":"
LDFLAGS="-xildoff ${USR_OPT_FLAGS:=$DEF_OPT_FLAGS}"

PM_PATH="/net/neptune/pubmed1/Cvs5"
NCBI_C_INCLUDE="$NCBI/ver0.0/ncbi/include"
NCBI_C_LIB="$NCBI/ver0.0/ncbi/altlib"

KeepStateTarget=".KEEP_STATE:"

sh configure --exec_prefix=${USR_BUILD_DIR:=$DEF_BUILD_DIR}
