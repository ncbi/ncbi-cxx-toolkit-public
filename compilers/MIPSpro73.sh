#! /bin/sh
#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:   MIPSpro 7.3
#   OS:         IRIX64 (6.5)
#   Processor:  mips
#
# $Revision$  // by Denis Vakatov, NCBI (vakatov@ncbi.nlm.nih.gov)
#############################################################################

## Setup for the local (NCBI) environment
set -a
NCBI_COMPILER="MIPSpro73"

#  32/64-bit architecture
case "$1" in
 32 )  ARCH_FLAG="-n32" ;;
 64 )  ARCH_FLAG="-64" ;;
 *  )  echo "USAGE: `basename $0` {32|64} [build_dir] [--configure-flags]"
     exit 1 ;;
esac
shift

#  Build directory (optional)
if test -n "$1" ; then
  case "$1" in
   -*) EXEC_PREFIX= ;;
   *)  EXEC_PREFIX="--exec-prefix=$1" ; shift ;;
  esac
fi

# Tools
CC="cc"
CXX="CC"
CXXCPP="$CXX -E -LANG:std"


## Configure
${CONFIG_SHELL-/bin/sh} `dirname $0`/../configure $EXEC_PREFIX "$@"
