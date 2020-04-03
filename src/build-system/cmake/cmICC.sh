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

# Look for the specified version in various reasonable places
# (tuned for NCBI's installations).
case "$1" in
  [1-9]*)
     if $intel_root/Compiler/$1/bin/$CXX -dumpversion >/dev/null 2>&1; then
       CXX=$intel_root/Compiler/$1/bin/$CXX
       CC=$intel_root/Compiler/$1/bin/$CC
     else
echo "ERROR:  cannot find ICC version $1; you may need to adjust PATH explicitly."
echo "or try one of these:"
ls $intel_root/Compiler
       exit 1
     fi
     shift
  ;;
esac

$CXX -dumpversion > /dev/null 2>&1
if test "$?" -ne 0 ; then
   cat <<EOF
ERROR:  cannot find ICC compiler ($CXX)
EOF
    exit 1
fi

export CC CXX
exec ${script_dir}/cmake-cfg-unix.sh --rootdir=$script_dir/../../.. --caller=$script_name "$@"
