#! /bin/sh
#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:   KAI C++ 4.0d
#   OS:         Solaris 2.8, Linux RH6.2
#   Processor:  SPARC,       Intel X86
#
# $Revision$  // by Denis Vakatov, NCBI (vakatov@ncbi.nlm.nih.gov)
#############################################################################


## Path to the compiler
CXX="KCC"
CC="$CXX"

$CXX -V
if test "$?" -ne 0 ; then
   cat <<EOF
ERROR:  cannot find KAI C++ compiler ($CXX)

HINT:  if you are at NCBI, try to add the following path:
 Linux:
   sh, bash:
      PATH="/usr/kcc/KCC_BASE/bin:\$PATH"
   tcsh:
      setenv PATH /usr/kcc/KCC_BASE/bin:\$PATH

EOF
    exit 1
fi


## 32- or 64-bit architecture
case "$1" in
 --help )  HELP="--help" ;;
 32     )  ARCH="" ;;
 64     )  ARCH="--with-64" ;;
 * )
    echo "USAGE: `basename $0` {32|64} [build_dir] [--configure-flags] [--help]"
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
export CC CXX

${CONFIG_SHELL-/bin/sh} `dirname $0`/../configure $HELP $EXEC_PREFIX $ARCH "$@"
