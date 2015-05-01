#! /bin/sh
# $Id$
# Author:  Vladimir Ivanov (ivanov@ncbi.nlm.nih.gov)
#
# Build C++ Toolkit using Cygwin


########### Arguments

script="$0"
cfgs="${1:-Debug DebugMT Release ReleaseMT}"
arch=${2}

 
########### Global variables

cmd_Debug='--with-debug --without-mt --without-dll'
cmd_DebugMT='--with-debug --with-mt --without-dll'
##cmd_DebugDLL='--with-debug --with-mt --with-dll'
cmd_Release='--without-debug --without-mt --without-dll'
cmd_ReleaseMT='--without-debug --with-mt --without-dll'
##cmd_ReleaseDLL='--without-debug --with-mt --with-dll'
cmd_common='--without-internal'

timer="date +'%H:%M'"


########## Functions

error()
{
  echo "[`basename $script`] ERROR:  $1"
  exit 1
}


########## Main

# Get build dir
build_dir=`dirname $script`
build_dir=`(cd "$build_dir"; pwd)`

if [ ! -d $build_dir ] ; then
    error "Build directory $build_dir not found"
fi


for cfg in $cfgs ; do
    cd $build_dir/../..  ||  error "Cannot change directory"

    # Configure

    start=`eval $timer`
    echo Start time: $start
    echo "INFO: Configure \"$cfg\""
    if [ $cfg = ReleaseDLL -o $cfg = DebugDLL ] ; then
       error "DLLs configurations are not buildable on this platform." 
    fi
    cmd=`eval echo "$"cmd_${cfg}""`
    ./configure $cmd $cmd_common
    if [ $? -ne 0 ] ; then
       exit 3
    fi
    echo "Build time: $start - `eval $timer`"

    # Build

    dir=`find . -maxdepth 1 -name "*-$cfg*" | head -1 | sed 's|^.*/||g'`
    if [ -z "$dir"  -o  ! -d "$dir" ] ; then
       error "Build directory for \"$cfg\" configuration not found"
    fi
    echo $dir >> $build_dir/cfgs.log
    start=`eval $timer`
    echo Start time: $start
    echo "INFO: Building \"$dir\""
    cd $dir/build  ||  error "Cannot change build directory"
    make all_r
    status=$?
    echo "Build time: $start - `eval $timer`"
    if [ $status -ne 0 ] ; then
       exit 4
   fi
done

exit 0
