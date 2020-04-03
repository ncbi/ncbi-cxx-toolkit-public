#! /bin/sh
#############################################################################
# $Id$
#   Compiler:   LLVM GCC
#   Find LLVM GCC compiler of a specific version and
#   call CMake configuration script
#
#############################################################################


script_dir=`dirname $0`
script_name=`basename $0`

## Path to the compiler
CXX="llvm-g++"
CC="llvm-gcc"

Usage() {
    echo "USAGE:   $script_name [version] [configure-flags] | -h]"
    echo "example: $script_name 7.0.0"
}

if test $# -eq 0 -o "$1" = "-h"; then
  Usage
  exit 0
fi

# Look for the specified version in various reasonable places
# (tuned for NCBI's installations).
case "$1" in
  [1-9]*)
     if /usr/local/llvm/$1/bin/$CXX -dumpversion >/dev/null 2>&1; then
       CXX=/usr/local/llvm/$1/bin/$CXX
       CC=/usr/local/llvm/$1/bin/$CC
     elif $CXX-$1 -dumpversion >/dev/null 2>&1; then
       CXX="$CXX-$1"
       CC="$CC-$1"
     else
echo "ERROR:  cannot find LLVM GCC version $1; you may need to adjust PATH explicitly."
echo "or try one of these:"
ls /usr/local/llvm
       exit 1
     fi
     shift
  ;;
esac

$CXX -dumpversion > /dev/null 2>&1
if test "$?" -ne 0 ; then
   cat <<EOF
ERROR:  cannot find LLVM GCC compiler ($CXX)
EOF
    exit 1
fi

export CC CXX
exec ${script_dir}/cmake-cfg-unix.sh --rootdir=$script_dir/../../.. --caller=$script_name "$@"
