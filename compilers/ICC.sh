#! /bin/sh
#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:   Intel C++ Version 5.0.1 Beta
#   OS:         Linux RH6.2
#   Processor:  Intel X86
#
# $Revision$  // Dmitriy Beloslyudtsev, NCBI (beloslyu@ncbi.nlm.nih.gov)
#############################################################################


NCBI_COMPILER="ICC"


## Path to the compiler
CXX="icc"
CC="$CXX"

$CXX -V -help >/dev/null 2>&1
if test "$?" -ne 0 ; then
   cat <<EOF
ERROR:  cannot find Intel C++ compiler ($CXX)

HINT:  if you are at NCBI, try to add the following path:
  Linux:
    setenv IA32ROOT /export/home/coremake/intel/compiler50/ia32
    setenv LD_LIBRARY_PATH /export/home/coremake/intel/compiler50/ia32/lib:\$LD_LIBRARY_PATH
    setenv INTEL_FLEXLM_LICENSE /export/home/coremake/intel/licenses/license.dat
    setenv PATH /export/home/coremake/intel/compiler50/ia32/bin:\$PATH

EOF
    exit 1
fi

## Build directory (optional)
if test -n "$1" ; then
  case "$1" in
   -* )  EXEC_PREFIX="" ;;
   *  )  EXEC_PREFIX="--exec-prefix=$1" ; shift ;;
  esac
fi


## Configure
export CC CXX NCBI_COMPILER

${CONFIG_SHELL-/bin/sh} `dirname $0`/../configure $HELP $EXEC_PREFIX $ARCH "$@"
