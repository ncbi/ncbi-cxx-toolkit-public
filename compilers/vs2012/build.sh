#! /bin/sh
# $Id$
# Author:  Vladimir Ivanov (ivanov@ncbi.nlm.nih.gov)
#
# Build C++ Toolkit.


#---------------- Arguments ----------------

script="$0"
cfgs="${1:-DebugDLL ReleaseDLL}"
arch="$2"


#---------------- Configuration ----------------

# Configure with Unicode configurations enabled

NCBI_CONFIG____ENABLEDUSERREQUESTS__NCBI_UNICODE=1
export NCBI_CONFIG____ENABLEDUSERREQUESTS__NCBI_UNICODE
#NCBI_CONFIG____ENABLEDUSERREQUESTS__TWEAKVTUNE=1
#export NCBI_CONFIG____ENABLEDUSERREQUESTS__TWEAKVTUNE


#---------------- Global variables ----------------

build_trees='static dll'
sol_static="ncbi_cpp.sln gui/ncbi_gui.sln"
sol_dll="ncbi_cpp_dll.sln gui/ncbi_gui_dll.sln"
timer="date +'%H:%M'"

# TRUE if parallell project build system is enabled in Visual Studio
is_ppb=false
need_ppb_check=true



#-------------- Functions --------------

error()
{
    echo "[`basename $script`] ERROR:  $1"
    exit 1
}

generate_vs2012_error_check_file()
{
    cat <<-EOF >$1
	/.*--* (Reb|B)uild( All | )started: Project:/ {
	  expendable = ""
	}

	/^--* Project:/ {
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

generate_simple_log()
{
    echo Parallel project build detected! Creating simplified log.
    echo
       
    log=$1
    tree=$2
    sol=$3
    cfg=$4

    # Get solution directory
    sol_dir=`echo $sol | sed 's%/[^/]*$%%'`
    if [ $sol_dir = $sol ] ; then
        sol_dir=''
    fi 

    # Get built projects
    projects=`grep '.*--* Build started:' $log | awk '{ sub(/^.* started:/, ""); gsub(/ /,"#"); print $0}'`

    for p in $projects ; do
        echo "------$p" | awk '{gsub(/[#]/," "); print}'
        prj_name=`echo $p | awk '{gsub(/[#,]/," "); print $2}'`
###        cfg=`echo $p | awk '{gsub(/[#,]/," "); print $4}'`

        # Get path for specified project name from solution
        s=`grep \"$prj_name\" $tree/build/$sol | awk '{gsub(/,/," "); print $4}' | sed -e 's%"%%g' -e 's%\\\%/%g' -e 's%.vcxproj%%'`

        target_dir=`echo $s | sed 's%/[^/]*$%%'`
        test $target_dir = $s  &&  target_dir=''
        target_name=`echo $s | sed 's%^.*/%%'`

        # Path to regular logfile for current project
        prj_log="$tree/build/$sol_dir/$target_dir/$cfg/$prj_name/$target_name.log"

        # Add it to new combined log
        if test ! -f "$prj_log" ; then
            # Not all projects have a log file in the ${prj_name} sub-directory
            prj_log_short="$tree/build/$sol_dir/$target_dir/$cfg/$target_name.log"
            if test ! -f "$prj_log_short" ; then
                echo "BUILD_SYSTEM_ERROR: Cannot find log file for this project: $prj_log"
                echo
                continue
            fi
            prj_log=$prj_log_short
        fi
        # Remove 3 first bytes from logfile (EF BB BF) and some garbage from a multi-projects build
        cat $prj_log | tr -d '\357\273\277' | sed 's/\( *\)1>/\1  /g'
        echo
    done
    grep '.*========== Build:' $log
    echo
}


#---------------- Main ----------------

# Get build dir
build_dir=`dirname $script`
build_dir=`(cd "$build_dir"; pwd)`

if [ ! -d $build_dir ] ; then
    error "Build directory $build_dir not found"
    exit 1
fi
cd $build_dir

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
    sols=`eval echo "$"sol_${tree}""`
    for sol in $sols ; do
        if test ! -f "$tree/build/$sol" ; then
            echo "INFO: Solution not found, skipped."
            continue
        fi
        alias=`echo $sol | sed -e 's|\\\\.*$||g' -e 's|_.*$||g'`
        start=`eval $timer`
        echo
        echo Start time: $start
        echo "INFO: Configure \"$tree\\$alias\""
        echo "Command line: " $build_dir/build_exec.bat "$tree\\build\\$sol" build "$arch" "$cfg_configure" "_CONFIGURE_" $out
        $build_dir/build_exec.bat "$tree\\build\\$sol" build "$arch" "$cfg_configure" "_CONFIGURE_" $out >/dev/null
        echo
        status=$?
        # Wait a bit to allow compiler to exit and flush logfile
        sleep 20
        if $need_ppb_check; then
            need_ppb_check=false
            grep '^1>------ Build started:' $out >/dev/null 2>&1  &&  is_ppb=true
        fi
        if $is_ppb; then
            generate_simple_log $out $tree "$sol" $cfg_configure > $out.simple
            mv $out $cfg.configure.log
            mv $out.simple $out
        fi 
        cat $out
        cat $out >> ${log_dir}/${tree}_${cfg_configure}.log
        echo "Build time: $start - `eval $timer`"
        echo STATUS = $status
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
generate_vs2012_error_check_file $check_awk


# Build

for tree in $build_trees ; do
    for cfg in $cfgs ; do
        if [ $tree = dll ] ; then
            echo "$cfg" | grep '.*DLL$' >/dev/null  ||  continue
        fi
        sols=`eval echo "$"sol_${tree}""`
        for sol in $sols ; do
            if test ! -f "$tree/build/$sol" ; then
                echo "INFO: Solution not found, skipped."
                continue
            fi
            alias=`echo $sol | sed -e 's|\\\\.*$||g' -e 's|_.*$||g'`
            start=`eval $timer`
            echo
            echo Start time: $start
            echo "$tree,$sol,$cfg" >> $build_dir/cfgs.log
            echo "INFO: Building \"$tree\\$cfg\\$alias\""
            echo "Command line: " $build_dir/build_exec.bat "$tree\\build\\$sol" build "$arch" "$cfg" "_BUILD_ALL_" $out
            echo
            $build_dir/build_exec.bat "$tree\\build\\$sol" build "$arch" "$cfg" "_BUILD_ALL_" $out >/dev/null
            status=$?
            # Wait a bit to allow compiler to exit and flush logfile
            sleep 20
            if $is_ppb; then
                generate_simple_log $out $tree "$sol" $cfg > $out.simple
                mv $out $cfg.build.log
                mv $out.simple $out
            fi 
            cat $out
            cat $out >> ${log_dir}/${tree}_${cfg}.log
            echo "Build time: $start - `eval $timer`"
            echo STATUS = $status
            if [ $status -ne 0 ] ; then
                # Check on errors (skip expendable projects)
                failed="1"
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
