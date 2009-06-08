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


########## Functions

error()
{
    echo "[`basename $script`] ERROR:  $1"
    exit 1
}


########## Main

errcode=0

# Get build directory
build_dir=`dirname $script`
build_dir=`(cd "$build_dir"; pwd)`

if [ ! -d $build_dir ] ; then
    error "Build directory $build_dir not found"
fi
cd $build_dir  ||  error "Cannot change directory"

res_log="$build_dir/check.sh.log"
res_concat="$build_dir/check.sh.out"
res_concat_err="$build_dir/check.sh.out_err"

cfgs="`cat cfgs.log`"
if [ -z "$cfgs" ] ; then
    error "Build some configurations first"
fi

# Initialization

case "$method" in
    run )
        rm -f "$res_log"
        rm -f "$build_dir/check.sh.*.log" > /dev/null 2>&1
        ;;
    clean )
        # not implemented, 'clean' method is not used with Xcode
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
        ;;
    * )
        error "Invalid method name"
        ;;
esac


# Run checks for each previously built configuration

for cfg in $cfgs ; do
    if [ -z "$cfg" ] ; then
        error "Unknown configuration name"
    fi
    cfg_params=`echo $cfg | sed -e 's/,/ /g'`
    cfg_count=1
    for cfg_p in $cfg_params; do
      case $cfg_count in
        1 ) x_tree=$cfg_p ;;
        2 ) x_sol=$cfg_p ;;
        3 ) x_cfg=$cfg_p ;;
      esac
      cfg_count=`expr $cfg_count + 1`
    done
    echo CHECK_$method: $x_tree/$x_sol/$x_cfg

    cd $build_dir
    check_dir="$x_tree/${x_sol}.check/$x_cfg"
    if [ ! -d "$check_dir" ] ; then
        error "Check directory \"$check_dir\" not found"
    fi
    if test "$method" != "run"; then
        test -x "$check_dir/check.sh"  ||  error "Run checks first. $check_dir/check.sh not found."
    fi

    # Action
    
    case "$method" in
        run )
            ../../scripts/common/check/check_make_cfg.sh XCODE create "$x_sol" "$x_tree" "$x_cfg"  || \
                error "Creating check script for \"$check_dir\" failed"
            $check_dir/check.sh run  ||  errcode=$?
            cat $check_dir/check.sh.log >> $res_log
            cat $check_dir/check.sh.log >> $build_dir/check.sh.${x_tree}_${x_cfg}.log
            ;;
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
            # see below copying of failed tests outputs
            ;;
        load_to_db )
            $check_dir/check.sh load_to_db
            ;;
    esac
done

if test "$method" = "concat_cfg"; then
    for cfg in $cfgs ; do
         cfg_params=`echo $cfg | sed -e 's/,/ /g'`
         cfg_count=1
         for cfg_p in $cfg_params; do
           case $cfg_count in
             1 ) x_tree=$cfg_p ;;
             2 ) x_sol=$cfg_p ;;
             3 ) x_cfg=$cfg_p ;;
           esac
           cfg_count=`expr $cfg_count + 1`
         done
         cd $build_dir
         check_dir="$x_tree/${x_sol}.check/$x_cfg"
         cat $check_dir/check.sh.out_err >> $build_dir/check.sh.${x_tree}_${x_cfg}.out_err
    done
fi

exit $errcode
