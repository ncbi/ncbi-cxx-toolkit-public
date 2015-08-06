#! /bin/sh
#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:   Intel C++
#   OS:         Linux
#   Processor:  Intel X86(-64)
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
    x86_64 ) arch=intel64 ;;
    *      ) arch=ia32    ;;
esac

if ls -d /usr/local/intel/[Cc]* >/dev/null 2>&1; then
    intel_root=/usr/local/intel
else
    intel_root=/opt/intel
fi

case "$1" in
  8           ) search=$intel_root/compiler8*/bin                ;;
  8.0         ) search=$intel_root/compiler80/bin                ;;
  [1-9].*.*   ) search=$intel_root/cc*/$1/bin                    ;;
  9* | 10*    ) search=$intel_root/cc*/$1.*/bin                  ;;
  11*         ) search=$intel_root/Compiler/$1*/*/bin/$arch      ;;
  12*         ) search=$intel_root/Compiler/$1/bin               ;;
  *13.[15]*   ) search=$intel_root/Compiler/13.5.192/bin/$arch   ;;
  13* | 2013  ) search=$intel_root/2013/bin                      ;;
  15* | 2015  ) search=$intel_root/2015/bin                      ;;
  *           ) search=                                          ;;
esac

if [ -n "$search" ]; then
    shift
    base_CC=$CC
    base_CXX=$CXX
    for dir in $search; do
        if test -x $dir/ncbi-wrappers/$base_CC; then
            CC=$dir/ncbi-wrappers/$base_CC
            CXX=$dir/ncbi-wrappers/$base_CXX
        elif test -x $dir/$base_CC; then
            CC=$dir/$base_CC
            CXX=$dir/$base_CXX
        fi
    done
fi

$CXX -V -help >/dev/null 2>&1
status=$?
if test "$status" -ne 0 ; then
   if test "$status" -eq 127; then
      echo "ERROR:  cannot find Intel C++ compiler ($CXX)."
   else
      echo "ERROR:  cannot run Intel C++ compiler ($CXX)."
      echo "Please check license (server) availability."
   fi
   cat <<EOF

HINT:  if you are at NCBI, try to run the following command:
   sh, bash:
      . $intel_root/Compiler/13.5.192/bin/iccvars.sh $arch
   tcsh:
      source $intel_root/Compiler/13.5.192/bin/iccvars.csh $arch

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
   -* | *=* )  BUILD_ROOT="" ;;
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
