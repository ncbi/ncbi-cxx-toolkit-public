#! /bin/sh
#############################################################################
# $Id$
#   Compiler:   Intel C++
#   Find ICC compiler of a specific version and
#   call CMake configuration script
#
#############################################################################


script_dir=`dirname $0`
script_name=`basename $0`
script_args="$@"

## Path to the compiler
CC="icc"
CXX="icpc"

Usage() {
    echo "USAGE:   $script_name [version] [configure-flags] | -h]"
    echo "example: $script_name 19.0"
}

if test $# -eq 0 -o "$1" = "-h"; then
  Usage
  exit 0
fi

arch=intel64

if ls -d /usr/local/intel/[Cc]* >/dev/null 2>&1; then
    intel_root=/usr/local/intel
else
    intel_root=/opt/intel
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
     if $intel_root/Compiler/$cxx_version/bin/$CXX -dumpversion >/dev/null 2>&1; then
       CXX=$intel_root/Compiler/$cxx_version/bin/$CXX
       CC=$intel_root/Compiler/$cxx_version/bin/$CC
       libICC=$intel_root/Compiler/$cxx_version/lib/intel64 
       gccver=7.3.0
     else
       if [ -x $intel_root/Compiler/$cxx_version/bin/$CXX ]; then
         $intel_root/Compiler/$cxx_version/bin/$CXX -dumpversion >/dev/null
       fi
       cat <<EOF 1>&2
ERROR:  cannot find ICC version $cxx_version; you may need to adjust PATH explicitly.
or try one of these:
EOF
ls $intel_root/Compiler 1>&2
       exit 1
     fi
  ;;
  *) 
    cat <<EOF 1>&2
ERROR:  cannot find ICC version $cxx_version
EOF
    exit 1
    ;; 
esac

$CXX -dumpversion > /dev/null 2>&1
if test "$?" -ne 0 ; then
   cat <<EOF 1>&2
ERROR:  cannot find ICC compiler ($CXX)
EOF
    exit 1
fi

# Intel C++ Compiler: GCC* Compatibility and Interoperability
# https://software.intel.com/en-us/cpp-compiler-developer-guide-and-reference-gcc-compatibility-and-interoperability

for gcc in /opt/ncbi/gcc/${gccver}/bin/gcc /usr/local/gcc/${gccver}/bin/gcc; do
  if test -x $gcc; then
    gccname="$gcc"
    case $gcc in
        /opt/* )
            lib64="/opt/ncbi/gcc/${gccver}/lib64"
            ;;
        /usr/* )
            lib64="/usr/lib64/gcc-${gccver}"
            ;;
    esac
    break
  fi
done
NCBI_COMPILER_C_FLAGS="-gcc-name=${gccname}"
NCBI_COMPILER_CXX_FLAGS="-gcc-name=${gccname}"
NCBI_COMPILER_EXE_LINKER_FLAGS="-gcc-name=${gccname} -Wl,-rpath,${lib64} -Wl,-rpath,${libICC}"
NCBI_COMPILER_SHARED_LINKER_FLAGS="-gcc-name=${gccname} -Wl,-rpath,${lib64} -Wl,-rpath,${libICC}"
export NCBI_COMPILER_C_FLAGS NCBI_COMPILER_CXX_FLAGS NCBI_COMPILER_EXE_LINKER_FLAGS NCBI_COMPILER_SHARED_LINKER_FLAGS
export CC CXX

if [ -z "$CMAKECFGRECURSIONGUARD" ]; then
  exec ${script_dir}/cmake-cfg-unix.sh --rootdir=$script_dir/../../.. --caller=$script_name "$@"
else
  exec ${script_dir}/cmake-cfg-unix.sh "$@"
fi
