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
script_args="$@"

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

platform="`uname -s``uname -r`-`uname -p`"
platform="`echo $platform | sed -e 's/SunOS5\./solaris/; s/\(sol.*\)-i386/\1-intel/'`"
oplatform=`echo $platform | sed -e 's/solaris11/solaris10/g'`

case "$cxx_version" in
  [1-9]*)
     # Look for the specified version in various reasonable places
     # (tuned for NCBI's installations).
     if /opt/ncbi/gcc/$cxx_version/bin/$CXX -dumpversion >/dev/null 2>&1; then
       CXX=/opt/ncbi/gcc/$cxx_version/bin/$CXX
       CC=/opt/ncbi/gcc/$cxx_version/bin/$CC
     elif /usr/local/gcc-$cxx_version/bin/$CXX -dumpversion >/dev/null 2>&1; then
       CXX=/usr/local/gcc-$cxx_version/bin/$CXX
       CC=/usr/local/gcc-$cxx_version/bin/$CC
     elif /usr/local/gcc/$cxx_version/bin/$CXX -dumpversion >/dev/null 2>&1; then
       CXX=/usr/local/gcc/$cxx_version/bin/$CXX
       CC=/usr/local/gcc/$cxx_version/bin/$CC
     elif /netopt/gcc/$cxx_version/$platform/bin/$CXX -dumpversion >/dev/null 2>&1; then
       CXX=/netopt/gcc/$cxx_version/$platform/bin/$CXX
       CC=/netopt/gcc/$cxx_version/$platform/bin/$CC
     elif /netopt/gcc/$cxx_version/$oplatform/bin/$CXX -dumpversion >/dev/null 2>&1; then
       CXX=/netopt/gcc/$cxx_version/$oplatform/bin/$CXX
       CC=/netopt/gcc/$cxx_version/$oplatform/bin/$CC
     elif $CXX-$cxx_version -dumpversion >/dev/null 2>&1; then
       CXX="$CXX-$cxx_version"
       CC="$CC-$cxx_version"
     else
       case "`$CXX -dumpversion 2>/dev/null`" in
         "$cxx_version" | "$cxx_version".* ) ;;
         * )
           cat <<EOF 1>&2
ERROR:  cannot find GCC version $cxx_version; you may need to adjust PATH explicitly.
or try one of these:
EOF
ls /opt/ncbi/gcc 1>&2
           exit 1
           ;;
       esac
     fi
    ;;
  *) 
    cat <<EOF 1>&2
ERROR:  cannot find GCC version $cxx_version
EOF
    exit 1
    ;; 
esac

$CXX -dumpversion > /dev/null 2>&1
if test "$?" -ne 0 ; then
   cat <<EOF 1>&2
ERROR:  cannot find GCC compiler ($CXX)
EOF
    exit 1
fi

export CC CXX
if [ -z "$CMAKECFGRECURSIONGUARD" ]; then
  exec ${script_dir}/cmake-cfg-unix.sh --rootdir=$script_dir/../../.. --caller=$script_name "$@"
else
  exec ${script_dir}/cmake-cfg-unix.sh "$@"
fi
