#! /bin/sh
#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:   Intel C++ Version 5.0.1 Beta
#   OS:         Linux RH6.2
#   Processor:  Intel X86
#
# $Revision$  // Dmitriy Beloslyudtsev, NCBI (beloslyu@ncbi.nlm.nih.gov)
#############################################################################


## Path to the compiler
CC="icc"
CXX="icpc"

$CXX -V -help >/dev/null 2>&1
if test "$?" -ne 0 ; then
   cat <<EOF
ERROR:  cannot find Intel C++ compiler ($CXX)

HINT:  if you are at NCBI, try to specify the following:
 Linux:
   sh, bash:
      PATH="/opt/intel/compiler60/ia32/bin:\$PATH"
      LD_LIBRARY_PATH="/opt/intel/compiler60/ia32/lib:\$LD_LIBRARY_PATH"
      export PATH LD_LIBRARY_PATH
   tcsh:
      setenv PATH /opt/intel/compiler60/ia32/bin:\$PATH
      setenv LD_LIBRARY_PATH /opt/intel/compiler60/ia32/lib:\$LD_LIBRARY_PATH

EOF
    exit 1
fi

## Build directory (optional)
if test -n "$1" ; then
  case "$1" in
   -* )  BUILD_ROOT="" ;;
   *  )  BUILD_ROOT="--with-build-root=$1" ; shift ;;
  esac
fi


## Configure
export CC CXX

${CONFIG_SHELL-/bin/sh} `dirname $0`/../configure $HELP $BUILD_ROOT $ARCH "$@"
