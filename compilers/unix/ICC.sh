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
case `uname -m` in
    x86_64 ) CXX=icpc ;;
    *      ) CXX="icpc -cxxlib-icc" ;;
esac

Usage() {
    echo "USAGE: `basename $0` [[build_dir] [--configure-flags] | -h]"
    exit $1
}

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

${CONFIG_SHELL-/bin/sh} `dirname $0`/../../configure $HELP $BUILD_ROOT "$@"
