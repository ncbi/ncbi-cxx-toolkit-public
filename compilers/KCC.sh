#! /bin/sh
#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:   KAI C++ 4.0d
#   OS:         Solaris 2.8, Linux RH6.2
#   Processor:  SPARC,       Intel X86
#
# $Revision$  // by Denis Vakatov, NCBI (vakatov@ncbi.nlm.nih.gov)
#############################################################################


NCBI_COMPILER="KCC"


## Path to the compiler
CXX="KCC"
CC="$CXX"

$CXX -V
if test "$?" -ne 0 ; then
   cat <<EOF
ERROR:  cannot find KAI C++ compiler ($CXX)

HINT:  if you are at NCBI, try to add the following path:
  Solaris:  setenv PATH /net/pluto/export/home/beloslyu/KCC/KCC_BASE/bin:\$PATH
  Linux:    setenv PATH /net/linus/export/home/coremake/KCC/KCC_BASE/bin:\$PATH

EOF
    exit 1
fi


## 32- or 64-bit architecture
case "$1" in
 --help )  HELP="--help" ;;
 32     )  ARCH="" ;;
 64     )  ARCH="--with-64" ;;
 * )
    echo "USAGE: $NCBI_COMPILER.sh {32|64} [build_dir] [--configure-flags] [--help]"
    exit 2
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
export CC CXX NCBI_COMPILER

${CONFIG_SHELL-/bin/sh} `dirname $0`/../configure $HELP $EXEC_PREFIX $ARCH "$@"
