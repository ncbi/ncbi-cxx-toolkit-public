#! /bin/sh
# $Id$
# Author:  Vladimir Ivanov (ivanov@ncbi.nlm.nih.gov)
#
# Build C++ Toolkit.


########### Arguments

script="$0"
cfgs="${1:-DebugDLL Debug ReleaseDLL Release}"
 
########### Global variables

build_trees='static dll'
sol_static="ncbi_cpp.sln gui\ncbi_gui.sln"
sol_dll="ncbi_cpp_dll.sln gui\ncbi_gui_dll.sln"
timer="date +'%H:%M'"

########## Functions

error()
{
    echo "[`basename $script`] ERROR:  $1"
    exit 1
}

generate_msvc7_error_check_file() {
    cat <<-EOF >$1
	/.*--* (Reb|B)uild( All | )started: Project:/ {
	  expendable = ""
	}

	/^EXPENDABLE project/ {
	  expendable = \$0
	}

	/(^| : |^The source )([fatal error ${filt_warn}]* [CDULNKPRJVT]*[0-9]*: |The .* are both configured to produce |Error executing )/ {
	if (!expendable) {
	  print \$0
	  exit
	  }
	}
	EOF
}


########## Main

# Get build dir
build_dir=`dirname $script`
build_dir=`(cd "$build_dir"; pwd)`

if [ ! -d $build_dir ] ; then
    error "Build directory $build_dir not found"
    exit 1
fi
cd $build_dir

# Configuration to build configure
cfg_configure=`echo $cfgs | sed 's| .*$||g'`
if [ -s $cfg_configure ] ; then
    error "Configuration to build configure is not specified"
    exit 2
fi
out=".build.$$"

chmod +x $build_dir/build_exec.bat
rm -f $build_dir/cfgs.log


# Configure

for tree in $build_trees ; do
    if [ $tree = dll ] ; then
        test $cfg_configure != ReleaseDLL -a $cfg_configure != DebugDLL  &&  continue  
    fi
    sols=`eval echo "$"sol_${tree}""`
    for sol in $sols ; do
        alias=`echo $sol | sed -e 's|\\\\.*$||g' -e 's|_.*$||g'`
        start=`eval $timer`
        echo Start time: $start
        echo "INFO: Configure \"$tree\\$alias\""
        echo "Command line: " $build_dir/build_exec.bat "$tree\\build\\$sol" build $cfg_configure "-CONFIGURE-"
        $build_dir/build_exec.bat "$tree\\build\\$sol" build $cfg_configure "-CONFIGURE-"
        if [ $? -ne 0 ] ; then
            exit 3
        fi
        echo "Build time: $start - `eval $timer`"
    done
done


# Generate errors check script

check_awk=$build_dir/build_check.awk
generate_msvc7_error_check_file $check_awk


# Build

for tree in $build_trees ; do
    for cfg in $cfgs ; do
        if [ $tree = dll ] ; then
            test $cfg != ReleaseDLL -a $cfg != DebugDLL  &&  continue  
        fi
        sols=`eval echo "$"sol_${tree}""`
        for sol in $sols ; do
            alias=`echo $sol | sed -e 's|\\\\.*$||g' -e 's|_.*$||g'`
            start=`eval $timer`
            echo Start time: $start
            echo "$tree,$sol,$cfg" >> $build_dir/cfgs.log
            echo "INFO: Building \"$tree\\$cfg\\$alias\""
            echo "Command line: " $build_dir/build_exec.bat "$tree\\build\\$sol" build $cfg "-BUILD-ALL-"
            $build_dir/build_exec.bat "$tree\\build\\$sol" build $cfg "-BUILD-ALL-" >$out 2>&1
            status=$?
            cat $out
            echo "Build time: $start - `eval $timer`"
            if [ $status -ne 0 ] ; then
                # Check on errors (skip expendable projects)
                failed="1"
                grep '^ *Build: .* succeeded, .* failed' $out >/dev/null 2>&1  && \
                    awk -f $check_awk $out >$out.res 2>/dev/null  &&  test ! -s $out.res  &&  failed="0"
                rm -f $out $out.res >/dev/null 2>&1
                if [ "$failed" = "1" ]; then
                    exit 4
                fi
            fi
            rm -f $out >/dev/null 2>&1
        done
    done
done


exit 0
