#! /bin/sh
#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:   LLVM Clang
#
# $Revision$  // by Aaron Ucko, NCBI (ucko@ncbi.nlm.nih.gov)
#############################################################################


## Path to the compiler
CXX="clang++"
CC="clang"

Usage() {
    echo "USAGE: `basename $0` [version] [[build_dir] [--configure-flags] | -h]"
    exit $1
}

case "$1" in
  [1-9].*)
     # Look for the specified version in various reasonable places
     # (tuned for NCBI's installations).
     if /usr/local/llvm/$1/bin/$CXX -dumpversion >/dev/null 2>&1; then
       CXX=/usr/local/llvm/$1/bin/$CXX
       CC=/usr/local/llvm/$1/bin/$CC
     elif $CXX-$1 -dumpversion >/dev/null 2>&1; then
       CXX="$CXX-$1"
       CC="$CC-$1"
     elif $CXX -v 2>&1 | fgrep -q " $1"; then
       :
     else
       cat <<EOF
ERROR:  cannot find Clang version $1; you may need to adjust PATH explicitly.
EOF
       exit 1
     fi
     shift
  ;;
esac


$CXX -dumpversion > /dev/null 2>&1
if test "$?" -ne 0 ; then
   cat <<EOF
ERROR:  cannot find Clang compiler ($CXX)
EOF
    exit 1
fi


## Build directory or help flags (optional)

if test -n "$1" ; then
  case "$1" in
   -h  )  Usage 0 ;;
   -* | *=* )  ;;
   32 | 64 )
          [ $1 = 32 ] && out=out
          cat <<EOF
ERROR: $0 does not accept "$1" as a positional argument.
Please supply --with$out-64 to force building of $1-bit binaries,
or --with-build-root=$1 if you wish to name your build root "$1".

EOF
          Usage 1 ;;
   *   )  BUILD_ROOT="--with-build-root=$1" ; shift ;;
  esac
fi


## Configure
export CC CXX

${CONFIG_SHELL-/bin/sh} `dirname $0`/../../configure $BUILD_ROOT "$@"
