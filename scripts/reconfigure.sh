#! /bin/sh

# $Id$
# Author:  Denis Vakatov, NCBI 
# 
#  Refresh makefiles and other configurables in the build tree.


###  Paths

script_name=`basename $0`
script_dir=`dirname $0`

if test -z "$builddir" ; then
  # temporary patch
  builddir=`pwd`
fi

if (set -P 2>/dev/null); then
    set_P='set -P'
else
    set_P=:
fi
top_srcdir=`($set_P ; cd "${script_dir}/.." ; pwd)`
status_dir=`(cd "${builddir}/../status" ; pwd)`

###  What to do (cmd-line arg)

method="$1"


###  Checks

if test -z "$builddir"  -o  ! -x "$top_srcdir/scripts/hello.sh"  -o  ! -f "$builddir/../inc/ncbiconf.h" ; then
  cat <<EOF

[$script_name]  ERROR:
  This script must be run only using its counterpart (which is also
  called "$script_name") located in the appropriate build directory,
  like this:
     ( cd $top_srcdir/GCC-Debug/build  &&  $script_name $method )

EOF
   exit 1
fi


###  Usage

if test $# -ne 1 ; then
  cat<<EOF
USAGE:  ./$script_name {recheck | reconf | update}

 "recheck"
    Exact equivalent of running the 'configure' script all over again,
    using just the same cmd.-line arguments as in the original 'configure'
    run, and without using cached check results from that run (but inherit
    the C and C++ compilers' path from the original 'configure' run).

 "reconf"
    Same as target "recheck" above, but do use cached check results
    obtained from the original 'configure' run.

 "update"
    Do just the same substitutions in just the same set of configurables
    ("*.in" files, mostly "Makefile.in") as the last run configuration did.
    Do not re-check the working environment (such as availability
    of 3rd-party packages), and do not process configurables which
    were added after the last full-scale run of 'configure'
    (or "recheck", or "reconfig").

EXAMPLE:
  It must be run from the appropriate build directory, like:
    ( cd c++/GCC-Debug/build  &&  ./$script_name reconf )
EOF
    exit 0
fi

# Check lock before potentially clobbering files.
if [ -f "$top_srcdir/configure.lock" ]; then
    cat $top_srcdir/configure.lock
    exit 1
fi

### Startup banner

cat <<EOF
Reconfiguring the NCBI C++ Toolkit in:
  - build  tree:  $builddir
  - source tree:  $top_srcdir
  - method:       $method

EOF


### Action

case "$method" in
  update )
    cd ${top_srcdir}  && \
    ${status_dir}/config.status
    ;;

  reconf )
    cd ${top_srcdir}  && \
    rm -f config.status config.cache config.log  && \
    cp -p ${status_dir}/config.status ${status_dir}/config.cache .  && \
    ./config.status --recheck  && \
    mv config.status config.cache config.log ${status_dir}  && \
    ${status_dir}/config.status

    # cd ${top_srcdir}  &&  rm -f config.status config.cache config.log
    ;;

  recheck )
    eval `sed '
        s|.*s%@CC@%\([^%][^%]*\)%.*|CC=\1;|p
        s|.*s%@CXX@%\([^%][^%]*\)%.*|CXX=\1|p
        d
    ' ${status_dir}/config.status`
    export CC CXX

    cd ${top_srcdir}  && \
    rm -f config.status config.cache config.log  && \
    cp -p ${status_dir}/config.status .  && \
    ./config.status --recheck  && \
    mv config.status config.cache config.log ${status_dir}  && \
    ${status_dir}/config.status

    # cd ${top_srcdir}  &&  rm -f config.status config.cache config.log
    ;;

  * )
    cat <<EOF
[$script_name]  ERROR:  Invalid method name.
  Help:  Method name must be one of:  "update", "reconf", "recheck".
  Hint:  Run this script without arguments to get full usage info.
EOF
    exit 1
    ;;
esac
