#! /bin/sh
#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:    WorkShop-6 C++ 5.1
#   OS:          Solaris 2.6 (or higher)
#   Processors:  Sparc,  Intel
#
# $Revision$  // by Denis Vakatov, NCBI (vakatov@ncbi.nlm.nih.gov)
#############################################################################


## Setup for the local (NCBI) environment
WS_PATH=${WS_BIN:="/netopt/forte6"}

set -a
NCBI_COMPILER="WorkShop6"

# 32- or 64-bit architecture (64-bit is for SPARC Solaris 2.7 only!)
case "$1" in
 32 | 32-bit | 32bit )
   WS64_CFLAGS= 
   LIBS="-Bstatic -L$WS_PATH/lib -lCrun -lm -Bdynamic"
   ;;
 64 | 64-bit | 64bit )
   WS64_CFLAGS="-xtarget=ultra -xarch=v9"
   LIBS="-Bstatic -L$WS_PATH/lib -lCrun -Bdynamic"
   ;;
 *)
   echo "USAGE: `basename $0` {32|64} [build_dir] [--configure-flags]"
   exit 1
   ;;
esac
shift

#  Build directory (optional)
if test -n "$1" ; then
  case "$1" in
   -*) EXEC_PREFIX= ;;
   *)  EXEC_PREFIX="--exec-prefix=$1" ; shift ;;
  esac
fi

CC="$WS_PATH/bin/cc"
CXX="$WS_PATH/bin/CC"

THREAD_LIBS="-mt -lthread -lpthread"


## Configure
${CONFIG_SHELL-/bin/sh} `dirname $0`/../configure $EXEC_PREFIX --with-internal "$@"
