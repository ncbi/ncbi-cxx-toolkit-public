#!/bin/sh
#
# $Id$
# Set tools and flags specific for SunPro 5.0 compilers, then run "configure"
#
# By Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#############################################################################


if test -z "$1" ; then
cat << EOF
USAGE: Go to the root c++ directory and run:
  ./compilers/`basename $0` build_dir [flags]

  build_dir           - name of the target build directory(e.g. just ".")
  flags (optional):
    --without-debug     - build the release versions of libs and apps
    --without-fastcgi   - explicitely prohibit the use of Fast-CGI library
    --without-internal  - do not build internal projects
EOF

exit 1
fi


USR_BUILD_DIR="$1"
DEF_BUILD_DIR="."

MT_FLAGS="-mt"

CPATH="/netopt/SUNWspro6/bin"
NCBI_C_PATH="$NCBI/ver0.0/ncbi"
NCBI_SSS_PATH="/net/neptune/pubmed1/sss/db"

set -a

SYBASE_PATH="/netopt/Sybase/clients/11.1.0"
PM_PATH="/net/neptune/pubmed1/Cvs5"

CC="$CPATH/cc"
CFLAGS="${MT_FLAGS}"
CXX="$CPATH/CC"
CXXFLAGS="+w +w2 -DNCBI_USE_NEW_HEADERS $MT_FLAGS"
AR="$CXX -xar -o"
RANLIB=":"
LDFLAGS="-xildoff"
LIBS="-Bstatic -L$CPATH/../SC5.0/lib -lm -Bdynamic -ldl"

NCBI_C_INCLUDE="$NCBI_C_PATH/include"
NCBI_C_DEBUG_LIB="$NCBI_C_PATH/altlib"
NCBI_C_RELEASE_LIB="$NCBI_C_PATH/lib"

NCBI_SSS_INCLUDE="$NCBI_SSS_PATH"
NCBI_SSS_DEBUG_LIB="$NCBI_SSS_PATH/Debug"
NCBI_SSS_RELEASE_LIB="$NCBI_SSS_PATH/Release"

THREAD_LIBS="-lthread -lpthread"
NETWORK_LIBS="-lsocket -lnsl"
FASTCGI_LIBS="-lfcgi5.0"

KeepStateTarget=".KEEP_STATE:"

if test $# -gt 0; then shift ; fi
sh configure --exec_prefix=${USR_BUILD_DIR:=$DEF_BUILD_DIR} --with-internal $*
