#! /bin/sh
#############################################################################
# $Id$
#   Compiler:   LLVM Clang
#   Find Clang compiler of a specific version and
#   call CMake configuration script
#
#############################################################################


script_dir=`dirname $0`
script_name=`basename $0`

## Path to the compiler
CXX="clang++"
CC="clang"

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
echo "ERROR:  cannot find Clang version $1; you may need to adjust PATH explicitly."
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
ERROR:  cannot find Clang compiler ($CXX)
EOF
    exit 1
fi

gccver=7.3.0
if test -x /opt/ncbi/gcc/${gccver}/bin/gcc; then
  inc1="/opt/ncbi/gcc/${gccver}/include/c++/${gccver}"
  inc2="/opt/ncbi/gcc/${gccver}/include/c++/${gccver}/x86_64-redhat-linux-gnu"
  inc3="/opt/ncbi/gcc/${gccver}/include/c++/${gccver}/backward"
  libgnu="/opt/ncbi/gcc/${gccver}/lib/gcc/x86_64-redhat-linux-gnu/${gccver}"
  lib64="/opt/ncbi/gcc/${gccver}/lib64"
  NCBI_COMPILER_C_FLAGS="-nostdinc++ -isystem ${inc1} -isystem ${inc2} -isystem ${inc3}"
  NCBI_COMPILER_CXX_FLAGS="-nostdinc++ -isystem ${inc1} -isystem ${inc2} -isystem ${inc3}"
  NCBI_COMPILER_EXE_LINKER_FLAGS="-L${libgnu} -B${libgnu} -L${lib64} -Wl,-rpath,${lib64}"
  NCBI_COMPILER_SHARED_LINKER_FLAGS="${NCBI_COMPILER_EXE_LINKER_FLAGS}"
fi
export NCBI_COMPILER_C_FLAGS NCBI_COMPILER_CXX_FLAGS NCBI_COMPILER_EXE_LINKER_FLAGS NCBI_COMPILER_SHARED_LINKER_FLAGS
export CC CXX

exec ${script_dir}/cmake-cfg-unix.sh --rootdir=$script_dir/../../.. --caller=$script_name "$@"
