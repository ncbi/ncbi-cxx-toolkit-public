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

Usage() {
    echo "USAGE: `basename $0` [version] [[build_dir] [--configure-flags] | -h]"
    exit $1
}

platform="`uname -s``uname -r`-`uname -p`"
platform="`echo $platform | sed -e 's/SunOS5\./solaris/; s/\(sol.*\)-i386/\1-intel/'`"
oplatform=`echo $platform | sed -e 's/solaris11/solaris10/g'`

case "$1" in
  [1-9].*)
     # Look for the specified version in various reasonable places
     # (tuned for NCBI's installations).
     if /usr/local/gcc-$1/bin/$CXX -dumpversion >/dev/null 2>&1; then
       CXX=/usr/local/gcc-$1/bin/$CXX
       CC=/usr/local/gcc-$1/bin/$CC
     elif /usr/local/gcc/$1/bin/$CXX -dumpversion >/dev/null 2>&1; then
       CXX=/usr/local/gcc/$1/bin/$CXX
       CC=/usr/local/gcc/$1/bin/$CC
     elif /netopt/gcc/$1/$platform/bin/$CXX -dumpversion >/dev/null 2>&1; then
       CXX=/netopt/gcc/$1/$platform/bin/$CXX
       CC=/netopt/gcc/$1/$platform/bin/$CC
     elif /netopt/gcc/$1/$oplatform/bin/$CXX -dumpversion >/dev/null 2>&1; then
       CXX=/netopt/gcc/$1/$oplatform/bin/$CXX
       CC=/netopt/gcc/$1/$oplatform/bin/$CC
     elif $CXX-$1 -dumpversion >/dev/null 2>&1; then
       CXX="$CXX-$1"
       CC="$CC-$1"
     elif $CXX-fsf-$1 -dumpversion >/dev/null 2>&1; then
       CXX="$CXX-fsf-$1"
       CC="$CC-fsf-$1"
     elif llvm-$CXX-$1 -dumpversion >/dev/null 2>&1; then
       CXX="llvm-$CXX-$1"
       CC="llvm-$CC-$1"
     elif test "`$CXX -V$1 -dumpversion 2>/dev/null`" = "$1"; then
       CXX="$CXX -V$1"
       CC="$CC -V$1"
     elif test "`$CXX -dumpversion 2>/dev/null`" \!= "$1"; then
       cat <<EOF
ERROR:  cannot find GCC version $1; you may need to adjust PATH explicitly.
EOF
       exit 1
     fi
     shift
  ;;
esac


$CXX -dumpversion > /dev/null 2>&1
if test "$?" -ne 0 ; then
   cat <<EOF
ERROR:  cannot find GCC compiler ($CXX)
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
