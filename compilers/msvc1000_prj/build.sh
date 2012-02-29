#! /bin/sh
# $Id$
# Author:  Vladimir Ivanov (ivanov@ncbi.nlm.nih.gov)
#
# Build C++ Toolkit.


########### Arguments

script="$0"
cfgs="${1:-DebugDLL ReleaseDLL}"
arch="$2"


########### Global variables

build_trees='static dll'
###sol_static="ncbi_cpp.sln"
###sol_dll="ncbi_cpp_dll.sln"
sol_static="ncbi_cpp.sln gui/ncbi_gui.sln"
sol_dll="ncbi_cpp_dll.sln gui/ncbi_gui_dll.sln"
timer="date +'%H:%M'"


########## Functions

error()
{
    echo "[`basename $script`] ERROR:  $1"
    exit 1
}

generate_msvc10_error_check_file() {
    cat <<-EOF >$1
	/.*--* (Reb|B)uild( All | )started: Project:/ {
	  expendable = ""
	}

	/EXPENDABLE project/ {
	  expendable = \$0
	}

	/(| : |The source )([fatal ]*error [A-Z]*[0-9]* *: |The .* are both configured to produce|.*: error [0-9]*:|: general error |Error executing |ERROR: This project depends)/ {
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

for cfg in $cfgs ; do
    if [ $cfg = Release -o $cfg = Debug ] ; then
       error "$cfg configuration is not buildable on this platform." 
    fi
done


# Configuration to build configure
cfg_configure='ReleaseDLL'
out=".build.$$"

# Get directory for build logfiles
log_dir="$build_dir/../../logs"
mkdir $log_dir >/dev/null 2>&1
log_dir=`(cd "$log_dir"; pwd)`
rm $log_dir/* >/dev/null 2>&1


chmod +x $build_dir/build_exec.bat
rm -f $build_dir/cfgs.log


# Configure

for tree in $build_trees ; do
    if [ $tree = dll ] ; then
        test $cfg_configure != ReleaseDLL -a $cfg_configure != DebugDLL  &&  continue  
    fi
    sols=`eval echo "$"sol_${tree}""`
    for sol in $sols ; do
        if test ! -f "$tree/build/$sol" ; then
            echo "INFO: Solution not found, skipped."
            continue
        fi
        alias=`echo $sol | sed -e 's|\\\\.*$||g' -e 's|_.*$||g'`
        start=`eval $timer`
        echo Start time: $start
        echo "INFO: Configure \"$tree\\$alias\""
        echo "Command line: " $build_dir/build_exec.bat "$tree\\build\\$sol" build "$arch" "$cfg_configure" "-CONFIGURE-" $out
        $build_dir/build_exec.bat "$tree\\build\\$sol" build "$arch" "$cfg_configure" "-CONFIGURE-" $out >/dev/null
        status=$?
        # Wait a bit to allow compiler to exit and flush logfile
        sleep 20
        cat $out
        cat $out >> ${log_dir}/${tree}_${cfg_configure}.log
        echo "Build time: $start - `eval $timer`"
        if [ $status -ne 0 ] ; then
            echo "FAILED: Configure $tree\\build\\$sol, $cfg_configure"
        fi
        rm -f $out >/dev/null 2>&1
        if [ $status -ne 0 ] ; then
            exit 3
        fi
    done
done


# Generate errors check script

check_awk=$build_dir/build_check.awk
generate_msvc10_error_check_file $check_awk


# Build

for tree in $build_trees ; do
    for cfg in $cfgs ; do
        if [ $tree = dll ] ; then
            test $cfg != ReleaseDLL -a $cfg != DebugDLL  &&  continue  
        fi
        sols=`eval echo "$"sol_${tree}""`
        for sol in $sols ; do
            if test ! -f "$tree/build/$sol" ; then
                echo "INFO: Solution not found, skipped."
                continue
            fi
            alias=`echo $sol | sed -e 's|\\\\.*$||g' -e 's|_.*$||g'`
            start=`eval $timer`
            echo Start time: $start
            echo "$tree,$sol,$cfg" >> $build_dir/cfgs.log
            echo "INFO: Building \"$tree\\$cfg\\$alias\""
            echo "Command line: " $build_dir/build_exec.bat "$tree\\build\\$sol" build "$arch" "$cfg" "-BUILD-ALL-" $out
            $build_dir/build_exec.bat "$tree\\build\\$sol" build "$arch" "$cfg" "-BUILD-ALL-" $out >/dev/null
            status=$?
            # Wait a bit to allow compiler to exit and flush logfile
            sleep 20
            cat $out
            cat $out >> ${log_dir}/${tree}_${cfg}.log
            echo "Build time: $start - `eval $timer`"
            if [ $status -ne 0 ] ; then
                # Check on errors (skip expendable projects)
                failed="1"
                grep '^==* Build: .* succeeded, .* failed' $out >/dev/null 2>&1  && \
                    awk -f $check_awk $out >$out.res 2>/dev/null  &&  test ! -s $out.res  &&  failed="0"
                if [ "$failed" = "1" ]; then
                    echo "FAILED: Build $tree\\build\\$sol, $cfg"
                    echo "FAILED: Build $tree\\build\\$sol, $cfg" > failed.build.log
                    echo     >> failed.build.log
                    cat $out >> failed.build.log
                    cat $tree/build/${sol}_watchers.txt > failed.watchers.log
                fi
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
