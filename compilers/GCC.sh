#! /bin/sh
#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:   GCC
#
# $Revision$  // by Denis Vakatov, NCBI (vakatov@ncbi.nlm.nih.gov)
#############################################################################


## Path to the compiler
CXX="g++"
CC="gcc"

$CXX --version > /dev/null 2>&1
if test "$?" -ne 0 ; then
   cat <<EOF
ERROR:  cannot find GCC compiler ($CXX)
EOF
    exit 1
fi


## Build directory or help flags (optional)

if test -n "$1" ; then
  case "$1" in
   -h )
    echo "USAGE: `basename $0` [build_dir [--configure-flags] | -h]"
    exit 0
    ;;
   --* )  ;;
   *   )  BUILD_ROOT="--with-build-root=$1" ; shift ;;
  esac
fi


## Configure
export CC CXX

${CONFIG_SHELL-/bin/sh} `dirname $0`/../configure $BUILD_ROOT "$@"
