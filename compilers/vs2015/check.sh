#! /bin/sh
# $Id$
# Author:  Vladimir Ivanov (ivanov@ncbi.nlm.nih.gov)
#
# Check C++ Toolkit in all previously built configurations
# (see 'cfgs.log', generated with 'build.sh' script).
#
# USAGE:
#     check.sh {run | concat | concat_err | concat_cfg | load_to_db}
#
# Use 'run' command first, than use other commands.
# For 'concat_cfg' -- use 'run', 'concat' and 'concat_err' commands first.



########### Arguments

script="$0"
method="$1"

# Maximum number of parallel running test configurations
max_tasks=2
# Sleep timeout between checks on finished 'run' tasks (seconds)
sleeptime=60


########## Functions

Error()
{
    echo "[`basename $script`] ERROR:  $1"
    exit 1
}

ParseConfig()
{
    if [ -z "$1" ] ; then
        Error "Unknown configuration name"
    fi
    x_tree=`echo $1 | sed -e 's/,.*$//'`
    x_sol=`echo $1 | sed -e 's/^[^,]*,//' -e 's/,.*$//' -e 's/\.sln//' -e 's|\\\|/|g'`
    x_cfg=`echo $1 | sed -e 's/^.*,//'`
}

CopyConfigLogs()
{
    cat ${tasks_dir[$1]}/check.sh.log >> $res_log
    cat ${tasks_dir[$1]}/check.sh.log >> \
        $build_dir/check.sh.${tasks_tree[$1]}_${tasks_cfg[$1]}.log
}



########## Main

errcode=0

# Get build directory
build_dir=`dirname $script`
build_dir=`(cd "$build_dir"; pwd)`
timer="date +'%H:%M'"

if [ ! -d $build_dir ] ; then
    Error "Build directory $build_dir not found"
fi
cd $build_dir  ||  Error "Cannot change directory"

res_log="$build_dir/check.sh.log"
res_concat="$build_dir/check.sh.out"
res_concat_err="$build_dir/check.sh.out_err"

cfgs="`cat cfgs.log`"
if [ -z "$cfgs" ] ; then
    Error "Build some configurations first"
fi


# --- Initialization

case "$method" in
    run )
        rm -f "$res_log"
        rm -f "$build_dir/check.sh.*.log" > /dev/null 2>&1
        # Init checks
        $build_dir/../../scripts/common/check/check_make_win_cfg.sh init  || \
            Error "Check initialization failed"
        # Init task list
        i=0
        while [ $i -lt $max_tasks ] ; do
            tasks_name[$i]=""
            tasks_tree[$i]=""
            tasks_cfg[$i]=""
            tasks_dir[$i]=""
            tasks_start[$i]=""
            i=`expr $i + 1`
        done
        ;;
    clean )
        # not implemented, 'clean' method is not used on Windows 
        exit 0
        ;;
    concat )
        cp $res_log $res_concat
        ;;
    concat_err )
        egrep 'ERR \[|TO  -' $res_log > $res_concat_err
        ;;
    concat_cfg )
        rm -f "$build_dir/check.sh.*.out_err" > /dev/null 2>&1
        ;;
    load_to_db )
        rm -f "$build_dir/test_stat_load.log" > /dev/null 2>&1
        ;;
    * )
        Error "Invalid method name"
        ;;
esac



# --- Run checks for each previously built configuration


for cfg in $cfgs ; do

    ParseConfig $cfg
    cd $build_dir

    check_name=$x_tree/$x_sol/$x_cfg
    check_dir="$x_tree/build/${x_sol}.check/$x_cfg"
    if [ ! -d "$check_dir" ] ; then
        Error "Check directory \"$check_dir\" not found"
    fi

    # Special processing for 'run' to allow parallel test runs

    if [ "$method" = "run" ]; then

        while true; do 
            i=0
            idx=999

            while [ $i -lt $max_tasks ]; do
                # Find first free slot
                if [ -z "${tasks_name[$i]}" ]; then
                    idx=$i
                    break
                fi
                # Check on finished tasks
                if [ -f "${tasks_dir[$i]}/check.success" ]; then
                    idx=$i
                fi
                if [ -f "${tasks_dir[$i]}/check.failed" ]; then
                    idx=$i
                    errcode=1
                fi
                if [ $idx -lt $max_tasks ]; then
                    CopyConfigLogs $idx
                    echo `eval $timer`
                    echo CHECK_$method: finished: ${tasks_name[$idx]} \(${tasks_start[idx]} - `eval $timer`\)
                    tasks_name[$idx]=""
                    break 
                fi
                i=`expr $i + 1`
            done
        
            if [ $idx -lt $max_tasks ]; then
               # Run tests for current configuration $cfg
               tasks_name[$idx]="$x_tree/$x_sol/$x_cfg"
               tasks_tree[$idx]="$x_tree"
               tasks_cfg[$idx]="$x_cfg"
               tasks_dir[$idx]="$check_dir"
               tasks_start[$idx]=`eval $timer`

               echo ${tasks_start[$idx]}
               echo CHECK_$method: started : ${tasks_name[$idx]}
               rm ${tasks_dir[$idx]}/check.success ${tasks_dir[$idx]}/check.failed >/dev/null 2>&1

               ../../scripts/common/check/check_make_win_cfg.sh create "$x_sol" "$x_tree" "$x_cfg"  || \
                   Error "Creating check script for \"$check_dir\" failed"
               test -x "${tasks_dir[$idx]}/check.sh"  || \
                   Error "Cannot find $check_dir/check.sh"

               ${tasks_dir[$idx]}/check.sh run >/dev/null 2>&1 &

               # Move to next configuration
               break
            else
               # All slots busy -- waiting
               sleep $sleeptime
            fi
        done

        continue
    fi


    # All actions except 'run'

    test -x "$check_dir/check.sh"  ||  \
        Error "Run checks first. $check_dir/check.sh not found."

    echo `eval $timer`
    echo CHECK_$method: $check_name
   
    case "$method" in
        concat )
            $check_dir/check.sh concat
            cat $check_dir/check.sh.out >> $res_concat
            ;;
        concat_err )
            $check_dir/check.sh concat_err
            cat $check_dir/check.sh.out_err >> $res_concat_err
            ;;
        concat_cfg )
            # Copy log entries
            egrep 'ERR \[|TO  -' $check_dir/check.sh.log >> $build_dir/check.sh.${x_tree}_${x_cfg}.out_err
            # See below for copying of failed tests outputs,
            # it should be printed after log entries for all configurations.
            ;;
        load_to_db )
            $check_dir/check.sh load_to_db
            ;;
    esac
done


# --- Waiting unfinished tasks for 'run'

if [ "$method" = "run" ]; then
    while true; do 
        i=0
        idx=999
        active=false

        while [ $i -lt $max_tasks ]; do
            if [ -n "${tasks_name[$i]}" ]; then
                # Check on finished tasks
                if [ -f "${tasks_dir[$i]}/check.success" ]; then
                    idx=$i
                fi
                if [ -f "${tasks_dir[$i]}/check.failed" ]; then
                    idx=$i
                    errcode=1
                fi
                if [ $idx -lt $max_tasks ]; then
                    CopyConfigLogs $idx
                    echo `eval $timer`
                    echo CHECK_$method: finished: ${tasks_name[$idx]} \(${tasks_start[$idx]} - `eval $timer`\)
                    tasks_name[$idx]=""
                    idx=999
                else
                    active=true
                fi
            fi
            i=`expr $i + 1`
        done

        if $active; then
           sleep $sleeptime
        else
           break
        fi
    done
fi

if [ "$method" = "concat_cfg" ]; then
    for cfg in $cfgs ; do
        ParseConfig $cfg
        cd $build_dir
        check_dir="$x_tree/build/${x_sol}.check/$x_cfg"
        cat $check_dir/check.sh.out_err >> $build_dir/check.sh.${x_tree}_${x_cfg}.out_err
    done
fi

exit $errcode
