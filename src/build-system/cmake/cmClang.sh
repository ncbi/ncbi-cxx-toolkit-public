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
script_args="$@"

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

cxx_version=""
if [ -z "$CMAKECFGRECURSIONGUARD" ]; then
  cxx_version=$1
else
  for val in ${script_args}
  do
  case "$val" in 
    [1-9]*)
      cxx_version=$val
      ;; 
    *) 
      ;; 
  esac 
  done
fi

if test -z "$cxx_version"; then
  echo $script_name $script_args 1>&2
  echo ERROR: compiler version was not specified 1>&2
  exit 1
fi

# Look for the specified version in various reasonable places
# (tuned for NCBI's installations).
case "$cxx_version" in
  [1-9]*)
     if /usr/local/llvm/$cxx_version/bin/$CXX -dumpversion >/dev/null 2>&1; then
       CXX=/usr/local/llvm/$cxx_version/bin/$CXX
       CC=/usr/local/llvm/$cxx_version/bin/$CC
     elif $CXX-$cxx_version -dumpversion >/dev/null 2>&1; then
       CXX="$CXX-$cxx_version"
       CC="$CC-$cxx_version"
     else
       cat <<EOF 1>&2
ERROR:  cannot find Clang version $cxx_version; you may need to adjust PATH explicitly.
or try one of these:
EOF
ls /usr/local/llvm 1>&2
       exit 1
     fi
  ;;
  *) 
    cat <<EOF 1>&2
ERROR:  cannot find Clang version $cxx_version
EOF
    exit 1
    ;; 
esac

$CXX -dumpversion > /dev/null 2>&1
if test "$?" -ne 0 ; then
   cat <<EOF 1>&2
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

if [ -z "$CMAKECFGRECURSIONGUARD" ]; then
  exec ${script_dir}/cmake-cfg-unix.sh --rootdir=$script_dir/../../.. --caller=$script_name "$@"
else
  exec ${script_dir}/cmake-cfg-unix.sh "$@"
fi
