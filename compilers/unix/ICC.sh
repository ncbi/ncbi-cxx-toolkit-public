#! /bin/sh
#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:   Intel C++ Version 8.0
#   OS:         Linux
#   Processor:  Intel X86
#
# $Revision$  // Dmitriy Beloslyudtsev, NCBI (beloslyu@ncbi.nlm.nih.gov)
#############################################################################


## Path to the compiler
CC="icc"
CXX="icpc"

Usage() {
    echo "USAGE: `basename $0` [version] [[build_dir] [--configure-flags] | -h]"
    exit $1
}

case "`uname -m`" in
    x86_64 ) bits=64 ;;
    *      ) bits=32 ;;
esac

case "$1" in
  8           ) search=/opt/intel/compiler8*/bin                ;;
  8.0         ) search=/opt/intel/compiler80/bin                ;;
  [1-9].*.*   ) search=/opt/intel/cc*/$1/bin                    ;;
  9* | 10     ) search=/opt/intel/cc*/$1.*/bin                  ;;
  [1-9][0-9]* ) search=/opt/intel/Compiler/$1*/*/bin/intel$bits ;;
  *           ) search=                                         ;;
esac

if [ -n "$search" ]; then
    shift
    base_CC=$CC
    base_CXX=$CXX
    for dir in $search; do
        if test -x $dir/$base_CC; then
            CC=$dir/$base_CC
            CXX=$dir/$base_CXX
        fi
    done
fi

$CXX -V -help >/dev/null 2>&1
if test "$?" -ne 0 ; then
   cat <<EOF
ERROR:  cannot find Intel C++ compiler ($CXX)

HINT:  if you are at NCBI, try to specify the following:
 Linux:
   sh, bash:
      PATH="/opt/intel/compiler80/bin:\$PATH"
      LD_LIBRARY_PATH="/opt/intel/compiler80/lib:\$LD_LIBRARY_PATH"
      export PATH LD_LIBRARY_PATH
      IA32ROOT="/opt/intel/compiler80"
      export IA32ROOT
      INTEL_LICENSE_FILE="/opt/intel/licenses/icc.lic"
      export INTEL_LICENSE_FILE
   tcsh:
      setenv PATH            /opt/intel/compiler80/bin:\$PATH
      setenv LD_LIBRARY_PATH /opt/intel/compiler80/lib:\$LD_LIBRARY_PATH
      setenv IA32ROOT        /opt/intel/compiler80
      setenv INTEL_LICENSE_FILE /opt/intel/licenses/icc.lic

EOF
    exit 1
fi

case `$CXX -dumpversion` in
  [1-8].* ) CXX="$CXX -cxxlib-icc" ;;
esac

## Build directory (optional)
if test -n "$1" ; then
  case "$1" in
   -h )  Usage 0 ;;
   -* )  BUILD_ROOT="" ;;
   32 | 64 )
         cat <<EOF
ERROR: $0 does not accept "$1" as a positional argument,
or non-default system ABIs.
Please supply --with-build-root=$1 if you wish to name your build root "$1".

EOF
         Usage 1 ;;
   *  )  BUILD_ROOT="--with-build-root=$1" ; shift ;;
  esac
fi


## Configure
export CC CXX

exec ${CONFIG_SHELL-/bin/sh} `dirname $0`/../../configure $HELP $BUILD_ROOT "$@"
