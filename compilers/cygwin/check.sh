#! /bin/sh
# $Id$
# Author:  Vladimir Ivanov (ivanov@ncbi.nlm.nih.gov)
#
# Check C++ Toolkit in all previously built configurations under Cygwin


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

# Get build dir
build_dir=`dirname $script`
build_dir=`(cd "$build_dir"; pwd)`

res_log="$build_dir/check.sh.log"
res_concat="$build_dir/check.sh.out"
res_concat_err="$build_dir/check.sh.out_err"


if [ ! -d $build_dir ] ; then
    error "Build directory $build_dir not found"
fi

cd $build_dir  ||  error "Cannot change directory"

cfgs="`cat cfgs.log`"
if [ -z "$cfgs" ] ; then
    error "Build some configurations first"
fi


case "$method" in
   run )
      rm -f "$res_log"
      ;;
   concat )
      rm -f "$res_concat"
      ;;
   concat_err )
      rm -f "$res_concat_err"
      ;;
   concat_cfg )
      rm -f $res_script.*.log
      rm -f $res_script.*.out_err
      ;;
esac


for cfg in $cfgs ; do
    cd $build_dir/../..  ||  error "Cannot change directory"

    if [ -z "$cfg"  -o  ! -d "$cfg" ] ; then
       error "Build directory for \"$cfg\" configuration not found"
    fi
    cd $cfg/build  ||  error "Cannot change build directory"
    x_cfg=`echo $cfg | sed 's|.*-||'`
    x_sed="s| --  \[| --  [${x_cfg}/|"

    case "$method" in
       run )
          make check_r RUN_CHECK=Y
          cat check.sh.log | sed "$x_sed" >> "$res_log"
          ;;
       load_to_db )
          ./check.sh load_to_db
          ;;
       clean )
          ./check.sh clean
          ;;
       concat )
          ./check.sh concat
          cat check.sh.out | sed "$x_sed" >> "$res_concat"
          echo >> "$res_concat"
          ;;
       concat_err )
          ./check.sh concat_err
          cat check.sh.out_err | sed "$x_sed" >> "$res_concat_err"
          echo >> "$res_concat_err"
          ;;
       concat_cfg )
          cp check.sh.log $build_dir/check.sh.${x_cfg}.log
          cp check.sh.out_err $build_dir/check.sh.${x_cfg}.out_err
          ;;
    esac
done

exit 0

    ### Action

case "$method" in
#----------------------------------------------------------
   run )
      ;;
#----------------------------------------------------------
   clean )
      # For all build trees
      for build_tree in $build_trees; do
         rm -rf $build_dir/$build_tree/check > /dev/null 2>&1
      done
      rm -f $res_journal $res_log $res_list $res_concat $res_concat.* $res_concat_err > /dev/null 2>&1
      rm -f $res_script > /dev/null 2>&1
      exit 0
      ;;
#----------------------------------------------------------
   concat )
      rm -f "$res_concat"
      ( 
      cat $res_log
      x_files=`cat $res_journal | sed -e 's/ /%gj_s4%/g'`
      for x_file in $x_files; do
         x_file=`echo "$x_file" | sed -e 's/%gj_s4%/ /g'`
         echo 
         echo 
         cat $x_file
      done
      ) >> $res_concat
      exit 0
      ;;
#----------------------------------------------------------
   concat_err )
      rm -f "$res_concat_err"
      ( 
      grep 'ERR \[' $res_log
      x_files=`cat $res_journal | sed -e 's/ /%gj_s4%/g'`
      for x_file in $x_files; do
         x_file=`echo "$x_file" | sed -e 's/%gj_s4%/ /g'`
         x_code=`grep -c '@@@ EXIT CODE:' $x_file`
         test $x_code -ne 0 || continue
         x_good=`grep -c '@@@ EXIT CODE: 0' $x_file`
         if test $x_good -ne 1 ; then
            echo 
            echo 
            cat $x_file
         fi
      done
      ) >> $res_concat_err
      exit 0
      ;;
#----------------------------------------------------------
   concat_cfg )
      rm -f $res_script.*.out
      for dir in $build_trees; do
         for cfg in $cfgs; do
            x_tests=`grep "\[$dir/$cfg/" $res_log`
            if [ -n "$x_tests" ]; then  
            ( 
            grep "\[$dir/$cfg/" $res_log > $res_script.${dir}_${cfg}.log
            grep "\[$dir/$cfg/" $res_log | grep 'ERR \['
            x_files=`grep "/$dir/check/$cfg/" $res_journal | sed -e 's/ /%gj_s4%/g'`
            for x_file in $x_files; do
              x_file=`echo "$x_file" | sed -e 's/%gj_s4%/ /g'`
              x_code=`grep -c '@@@ EXIT CODE:' $x_file`
              test $x_code -ne 0 || continue
              x_good=`grep -c '@@@ EXIT CODE: 0' $x_file`
              if test $x_good -ne 1 ; then
                 echo 
                 echo 
                 cat $x_file
              fi
            done
            ) >> $res_script.${dir}_${cfg}.out_err
            fi
         done
      done
      exit 0
      ;;

#----------------------------------------------------------
   * )
      Usage "Invalid method name."
      ;;
esac

