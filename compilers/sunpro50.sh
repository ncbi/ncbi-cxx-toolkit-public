#!/bin/sh
#
# $Id$
# Set tools and flags specific for SunPro 5.0 compilers, then run "configure"
#
# By Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#############################################################################

USR_BUILD_DIR="$1"
DEF_BUILD_DIR="."

MT_FLAGS="-mt"

set -a

CPATH="/netopt/SUNWspro6/bin"
PM_PATH="/net/neptune/pubmed1/Cvs5"
NCBI_C_PATH="$NCBI/ver0.0/ncbi"


CC="$CPATH/cc"
CFLAGS="-Xc ${MT_FLAGS}"
CXX="$CPATH/CC"
CXXFLAGS="+w +w2 -DNCBI_USE_NEW_HEADERS ${MT_FLAGS}"
AR="$CXX -xar -o"
RANLIB=":"
LDFLAGS="-xildoff"
LIBS="-Bstatic -L$CPATH/../SC5.0/lib -lm -Bdynamic -ldl"

NCBI_C_INCLUDE="$NCBI_C_PATH/include"
NCBI_C_DEBUG_LIB="$NCBI_C_PATH/altlib"
NCBI_C_RELEASE_LIB="$NCBI_C_PATH/lib"

THREAD_LIBS="-lthread -lpthread"
NETWORK_LIBS="-lsocket -lnsl"
FASTCGI_LIBS="-lfcgi5.0"

KeepStateTarget=".KEEP_STATE:"

if test $# -gt 0; then shift ; fi
sh configure --exec_prefix=${USR_BUILD_DIR:=$DEF_BUILD_DIR} $*
