#! /bin/posix/sh
#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:   Compaq C++
#   OS:         Tru64 (formerly OSF/1 and Digital Unix)
#   Processor:  alpha
#
# $Revision$  // by Aaron Ucko, NCBI (ucko@ncbi.nlm.nih.gov)
#############################################################################


#  Build directory (optional)
if test -n "$1" ; then
  case "$1" in
   -* )  BUILD_ROOT= ;;
   *  )  BUILD_ROOT="--with-build-root=$1" ;  shift ;;
  esac
fi


# Tools
CC="cc"
CXX="cxx"
# GNU binutils don't always properly handle the C++ compiler's output,
# so force use of Compaq's versions.
AR='PATH=/usr/bin:$$PATH /usr/bin/ar cr'
LORDER='PATH=/usr/bin:$$PATH /usr/bin/lorder'
RANLIB=/usr/bin/ranlib


## Configure
export CC CXX

## Put AR, LORDER, and RANLIB on the command line rather than in the
## environment so that "reconfigure.sh recheck" won't lose them.
${CONFIG_SHELL-/bin/sh} `dirname $0`/../../configure $BUILD_ROOT \
AR="$AR" LORDER="$LORDER" RANLIB="$RANLIB" "$@"
