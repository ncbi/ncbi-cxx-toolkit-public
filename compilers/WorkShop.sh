#! /bin/sh
#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:    WorkShop 5.0, 5.1, 5.2
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
if test ! -x "$CXX" ; then
    echo "ERROR:  cannot find WorkShop C++ compiler at:"
    echo "  $CXX"
    exit 1
fi


## 5.0, 5.1, or 5.2?
CC_version=`CC -V 2>&1`
case "$CC_version" in
 "CC: WorkShop Compilers 5"* )
    NCBI_COMPILER="WorkShop5"
    ;;
 "CC: Sun WorkShop 6"* )
    NCBI_COMPILER="WorkShop6"
    ;;
 "CC: Sun WorkShop 6 update 1 C\+\+ 5.2"* )
    NCBI_COMPILER="WorkShop52"
    ;;
 * )
    echo "ERROR:  unknown version of WorkShop C++ compiler:"
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
   -* )  EXEC_PREFIX="" ;;
   *  )  EXEC_PREFIX="--exec-prefix=$1" ; shift ;;
  esac
fi


## Configure
export CC CXX

${CONFIG_SHELL-/bin/sh} `dirname $0`/../configure $HELP $EXEC_PREFIX $ARCH --with-internal "$@"
