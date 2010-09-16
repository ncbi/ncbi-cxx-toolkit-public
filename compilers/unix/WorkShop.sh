#! /bin/sh
#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:    WorkShop 5.0, 5.1, 5.2, 5.3, 5.4
#   OS:          Solaris 2.6 (or higher)
#   Processors:  Sparc,  Intel
#
# $Revision$  // by Denis Vakatov, NCBI (vakatov@ncbi.nlm.nih.gov)
#############################################################################


## Path to the compiler
if test -z "$WS_BIN" ; then
    WS_BIN="`which CC  2>/dev/null`"
    if test ! -x "$WS_BIN" ; then
        WS_BIN="`type CC | sed 's/.* \([^ ]*\)$/\1/'`"
    fi
    WS_BIN=`dirname $WS_BIN`
fi

CC="$WS_BIN/cc"
CXX="$WS_BIN/CC"
CCC="$CXX"
if test ! -x "$CXX" ; then
    echo "ERROR:  cannot find WorkShop C++ compiler at:"
    echo "  $CXX"
    exit 1
fi


## Currently supported version(s)
CC_version=`$CXX -V 2>&1`
case "$CC_version" in
 "CC: Sun C++ 5.9"* )
    NCBI_COMPILER="WorkShop59"
    ;;
 "CC: Sun C++ 5.10"* )
    NCBI_COMPILER="WorkShop510"
    ;;
 "CC: Sun C++ 5.11"* )
    NCBI_COMPILER="WorkShop511"
    ;;
 * )
    echo "ERROR:  unsupported version of WorkShop C++ compiler:"
    echo "  $CXX -V -->  $CC_version"
    exit 2
    ;;
esac


## 32- or 64-bit architecture (64-bit is for SPARC Solaris 2.7 only!)
case "$1" in
 --help )  HELP="--help" ;;
 32     )  ARCH="" ;;
 64     )  ARCH="--with-64" ;;
 * )
    echo "USAGE: $NCBI_COMPILER.sh {32|64} [build_dir] [--configure-flags] [--help]"
    exit 3
    ;;
esac
shift


## Build directory (optional)
if test -n "$1" ; then
  case "$1" in
   -* )  BUILD_ROOT="" ;;
   *  )  BUILD_ROOT="--with-build-root=$1" ; shift ;;
  esac
fi


## Configure
export CC CXX CCC

${CONFIG_SHELL-/bin/sh} `dirname $0`/../../configure $HELP $BUILD_ROOT $ARCH "$@"
