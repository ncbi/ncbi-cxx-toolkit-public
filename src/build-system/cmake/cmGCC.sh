#! /bin/sh
#############################################################################
# $Id$
#   Compiler:   GCC
#   Find GCC compiler of a specific version and
#   call CMake configuration script
#
#############################################################################


script_dir=`dirname $0`
script_name=`basename $0`

## Path to the compiler
CXX="g++"
CC="gcc"

Usage() {
    echo "USAGE:   $script_name [version] [configure-flags] | -h]"
    echo "example: $script_name 7.3.0"
}

if test $# -eq 0 -o "$1" = "-h"; then
  Usage
  exit 0
fi

platform="`uname -s``uname -r`-`uname -p`"
platform="`echo $platform | sed -e 's/SunOS5\./solaris/; s/\(sol.*\)-i386/\1-intel/'`"
oplatform=`echo $platform | sed -e 's/solaris11/solaris10/g'`

case "$1" in
  [1-9].*)
     # Look for the specified version in various reasonable places
     # (tuned for NCBI's installations).
     if /opt/ncbi/gcc/$1/bin/$CXX -dumpversion >/dev/null 2>&1; then
       CXX=/opt/ncbi/gcc/$1/bin/$CXX
       CC=/opt/ncbi/gcc/$1/bin/$CC
     elif /usr/local/gcc-$1/bin/$CXX -dumpversion >/dev/null 2>&1; then
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
     else
       case "`$CXX -dumpversion 2>/dev/null`" in
         "$1" | "$1".* ) ;;
         * )
           cat <<EOF
ERROR:  cannot find GCC version $1; you may need to adjust PATH explicitly.
or try one of these:
EOF
ls /opt/ncbi/gcc
           exit 1
           ;;
       esac
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

export CC CXX
exec ${script_dir}/cmake-cfg-unix.sh --rootdir=$script_dir/../../.. --caller=$script_name "$@"
