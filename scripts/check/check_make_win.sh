#! /bin/sh

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Compile a check script and copy necessary files to run tests in the 
# MS VisualC++ build tree.
#
# Usage:
#    check_make_win.sh <in_test_list> <out_check_script> <build_dir> <cfgs>
#
#    in_test_list     - list of tests (it building with "make check_r"
#                       under UNIX)
#                       (default: "<build_dir>/check.sh.list")
#    out_check_script - full path to the output test script
#                       (default: "<build_dir>/check.sh")
#    build_dir        - path to MSVC build tree like".../msvc_prj"
#                       (default: will try determine path from current work
#                       directory -- root of build tree ) 
#    cfgs             - list of tests configurations 
#                       (default: "Debug DebugDLL Release ReleaseDLL")
#
#    If any parameter is skipped that will be used default value for it.
#
# Note:
#    - Work with NCBI MS VisualC++ build tree only.
#    - If first two parameters are skipped that this script consider what 
#      files <in_test_list> and <out_check_script> must be standing in the 
#      root of the MSVC build tree.
#
###########################################################################


# Process and check parameters

# Field delimiters in the list (this symbols used directly in the "sed" command)
x_delim=" ____ "
x_delim_internal="~"
x_tmp="/tmp"

x_list=$1
x_out=$2
x_build_dir=$3
x_confs="${4:-Debug DebugDLL Release ReleaseDLL}"

x_build_trees="." 
test ".$COMPILER" = ".msvc7"  &&  x_build_trees="static dll" 

if test ! -z "$x_build_dir"; then
   if test ! -d "$x_build_dir"; then
      echo "Build directory \"$x_build_dir\" don't exist."
      exit 1 
   fi
   # Expand path and remove trailing slash
   x_build_dir=`(cd "$x_build_dir"; pwd | sed -e 's/\/$//g')`
else
   # Get build dir name from current work directory
   x_build_dir=`pwd`
fi

x_root_dir=`echo "$x_build_dir" | sed -e 's%/compilers/.*$%%'`

if test -z "$x_list"; then
   x_list="$x_build_dir/check.sh.list"
fi

if test -z "$x_out"; then
   x_out="$x_build_dir/check.sh"
fi

x_script_name=`echo "$x_out" | sed -e 's%^.*/%%'`


# Check list
if test ! -f "$x_list"; then
   echo "Check list file \"$x_list\" not found."
   exit 1 
fi

# Change script's command interpreter from /bin/sh to /bin/bash.
# Cygwin's shell don't works correctly with process pids.
# echo "Changing scripts command interpreter..."
script_dirs="scripts"
tmp="$x_tmp/check_make_win.$$"

for d in $script_dirs; do
    script_list=`find $x_root_dir/$d -name '*.sh'`
    for s in $script_list; do
        echo $s | grep 'check_make_win.sh' > /dev/null 2>&1  &&  continue
        grep '^#! */bin/sh' $s > /dev/null 2>&1
        if test $? -eq 0; then
           cp -fp $s $tmp
           sed -e 's|^#! */bin/sh.*$|#! /bin/bash|' $s > $tmp
           touch -r $s $tmp
           cp -fp $tmp $s
           rm -f $tmp
        fi
    done
done


#//////////////////////////////////////////////////////////////////////////

cat > $x_out <<EOF
#! /bin/sh

root_dir="$x_root_dir"
build_dir="$x_build_dir"
src_dir="\$root_dir/src"
build_trees="$x_build_trees" 

res_script="$x_out"
res_journal="\$res_script.journal"
res_log="\$res_script.log"
res_list="\$res_script.list"
res_concat="\$res_script.out"
res_concat_err="\$res_script.out_err"

cfgs='Debug DebugDLL DebugMT Release ReleaseDLL ReleaseMT'


##  Printout USAGE info and exit

Usage() {
   cat <<EOF_usage

USAGE:  $x_script_name {run | clean | concat | concat_err}

 run         Run the tests. Create output file ("*.out") for each test, 
             plus journal and log files. 
 clean       Remove all files created during the last "run" and this script 
             itself.
 concat      Concatenate all files created during the last "run" into one big 
             file "\$res_log".
 concat_err  Like 'concat'. But into the file "\$res_concat_err" 
             will be added outputs of failed tests only.
 concat_cfg  Like 'concat_err'. But instead of using single big file some
             files with names "\$res_script.<tree>_<cfg>.out_err" will
             be created.

ERROR:  \$1
EOF_usage

    exit 1
}

if test \$# -ne 1 ; then
   Usage "Invalid number of arguments."
fi


###  What to do (cmd-line arg)

method="\$1"


### Action

case "\$method" in
#----------------------------------------------------------
   run )
      ;;
#----------------------------------------------------------
   clean )
      # For all build trees
      for build_tree in \$build_trees; do
         rm -rf \$build_dir/\$build_tree/check > /dev/null 2>&1
      done
      rm -f \$res_journal \$res_log \$res_list \$res_concat \$res_concat.* \$res_concat_err > /dev/null 2>&1
      rm -f \$res_script > /dev/null 2>&1
      exit 0
      ;;
#----------------------------------------------------------
   concat )
      rm -f "\$res_concat"
      ( 
      cat \$res_log
      x_files=\`cat \$res_journal | sed -e 's/ /%gj_s4%/g'\`
      for x_file in \$x_files; do
         x_file=\`echo "\$x_file" | sed -e 's/%gj_s4%/ /g'\`
         echo 
         echo 
         cat \$x_file
      done
      ) >> \$res_concat
      exit 0
      ;;
#----------------------------------------------------------
   concat_err )
      rm -f "\$res_concat_err"
      ( 
      grep 'ERR \[' \$res_log
      x_files=\`cat \$res_journal | sed -e 's/ /%gj_s4%/g'\`
      for x_file in \$x_files; do
         x_file=\`echo "\$x_file" | sed -e 's/%gj_s4%/ /g'\`
         x_code=\`grep -c '@@@ EXIT CODE:' \$x_file\`
         test \$x_code -ne 0 || continue
         x_good=\`grep -c '@@@ EXIT CODE: 0' \$x_file\`
         if test \$x_good -ne 1 ; then
            echo 
            echo 
            cat \$x_file
         fi
      done
      ) >> \$res_concat_err
      exit 0
      ;;
#----------------------------------------------------------
   concat_cfg )
      rm -f \$res_script.*.out
      for dir in \$build_trees; do
         for cfg in \$cfgs; do
            x_tests=\`grep "\[\$dir/\$cfg/" \$res_log\`
            if [ -n "\$x_tests" ]; then  
            ( 
            grep "\[\$dir/\$cfg/" \$res_log > \$res_script.\${dir}_\${cfg}.log
            grep "\[\$dir/\$cfg/" \$res_log | grep 'ERR \['
            x_files=\`grep "/\$dir/check/\$cfg/" \$res_journal | sed -e 's/ /%gj_s4%/g'\`
            for x_file in \$x_files; do
              x_file=\`echo "\$x_file" | sed -e 's/%gj_s4%/ /g'\`
              x_code=\`grep -c '@@@ EXIT CODE:' \$x_file\`
              test \$x_code -ne 0 || continue
              x_good=\`grep -c '@@@ EXIT CODE: 0' \$x_file\`
              if test \$x_good -ne 1 ; then
                 echo 
                 echo 
                 cat \$x_file
              fi
            done
            ) >> \$res_script.\${dir}_\${cfg}.out_err
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

# Run

# Export some global vars
top_srcdir="\$root_dir"
export top_srcdir

# Add current, build and scripts directories to PATH
PATH=".:\${build_dir}:\${root_dir}/scripts:\${PATH}"
export PATH

# Define time-guard script to run tests from other scripts
CHECK_EXEC="\${root_dir}/scripts/check/check_exec_test.sh"
CHECK_EXEC_STDIN="\$CHECK_EXEC -stdin"
export CHECK_EXEC
export CHECK_EXEC_STDIN

count_ok=0
count_err=0
count_absent=0
configurations="$x_confs"

rm -f "\$res_journal"
rm -f "\$res_log"

##  Run one test

RunTest() {
   # Parameters
   x_wdir="\$1"
   x_test="\$2"
   x_app="\$3"
   x_run="\$4"
   x_ext="\$5"
   x_timeout="\$6"
   x_requires="\$7"
   x_conf="\$8"

   x_work_dir="\$build_dir/\${build_tree}check/\$x_conf/\$x_wdir"
   mkdir -p \$x_work_dir

   # Check application requirements
   for x_req in \$x_requires; do
      (echo " \$FEATURES " | grep " \$x_req " > /dev/null)  ||  return 0
   done

   # Determine test application name
   x_path_run="\$build_dir/\${build_tree}bin/\$x_conf"
   result=1
   x_path_app="\$x_path_run/\$x_app"
   if test ! -f "\$x_path_app"; then
      x_path_app="\$x_path_run/\$x_test"
      if test ! -f "\$x_path_app"; then
         result=0
      fi
   fi

   x_cmd="[\${build_tree}\$x_conf/\$x_wdir]"
   if test \$result -eq 0; then
      echo "ABS --  \$x_cmd - \$x_app"
      echo "ABS --  \$x_cmd - \$x_app" >> \$res_log
      count_absent=\`expr \$count_absent + 1\`
      return 0
   fi

   # Generate name of the output file
   x_test_out="\$x_work_dir/\$x_app.\$x_ext"

   # Write header to output file 
   echo "\$x_test_out" >> \$res_journal
   (
      echo "======================================================================"
      echo "\${build_tree}\$x_conf - \$x_run"
      echo "======================================================================"
      echo 
   ) > \$x_test_out 2>&1

   # Goto the work directory 
   cd "\$x_work_dir"

   # Fix empty parameters (replace "" to \"\", '' to \'\')
   x_run_fix=\`echo "\$x_run" | sed -e 's/""/\\\\\\\\\\"\\\\\\\\\\"/g' -e "s/''/\\\\\\\\\\'\\\\\\\\\\'/g"\`
   # Fix empty parameters (put each in '' or "")
   x_run_fix=\`echo "\$x_run" | sed -e 's/""/'"'&'/g" -e "s/''/\\\\'\\\\'/g"\`

   # Run check
   CHECK_TIMEOUT="\$x_timeout"
   export CHECK_TIMEOUT
   start_time="\`date\`"
   check_exec="\$root_dir/scripts/check/check_exec.sh"
   \$check_exec time.exe -p \`eval echo \$x_run_fix\` > \$x_test_out.\$\$ 2>&1
   result=\$?
   stop_time="\`date\`"

   sed -e '/ ["][$][@]["].*\$/ {
      s/^.*: //
      s/ ["][$][@]["].*$//
      }' \$x_test_out.\$\$ >> \$x_test_out

   # Get application execution time
   exec_time=\`tail -3 \$x_test_out.\$\$\`
   exec_time=\`echo \$exec_time | tr '\n' '?'\`
   exec_time=\`echo \$exec_time | sed -e 's/?$//' -e 's/?/, /g' -e 's/[ ] */ /g'\`
   rm -f $x_tmp/\$\$.out
   rm -f \$x_test_out.\$\$

   # Write result of the test into the his output file
   echo "Start time: \$start_time" >> \$x_test_out
   echo "Stop  time: \$stop_time" >> \$x_test_out
   echo >> \$x_test_out
   echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" >> \$x_test_out
   echo "@@@ EXIT CODE: \$result" >> \$x_test_out

   # And write result also on the screen and into the log
   x_cmd="\$x_cmd \$x_run"
   if test \$result -eq 0; then
      echo "OK  --  \$x_cmd     (\$exec_time)"
      echo "OK  --  \$x_cmd     (\$exec_time)" >> \$res_log
      count_ok=\`expr \$count_ok + 1\`
   else
      echo "ERR --  \$x_cmd     (\$exec_time)"
      echo "ERR [\$result] --  \$x_cmd     (\$exec_time)" >> \$res_log
      count_err=\`expr \$count_err + 1\`
   fi
}

# For all build trees
for build_tree in \$build_trees; do

   if test "\$build_tree" = "."; then
      build_tree=""
   else
      build_tree="\$build_tree/"
   fi

   # Save value of PATH environment variable
   saved_path="\$PATH"

   # For each configuration
   for x_conf in \$configurations; do

   if test "\$build_tree" = "dll/" -a ! \$x_conf = "DebugDLL" -a ! \$x_conf = "ReleaseDLL" ; then
      continue
   fi

   # Features detection
   fs=\`cat \${build_dir}/\${build_tree}/\${x_conf}/features_and_packages.txt\`
   FEATURES=""
   for f in \$fs; do
      FEATURES="\$FEATURES\$f "
   done
   export FEATURES

   # Add current configuration's build and dll build directories to PATH
   PATH=".:\${build_dir}/\${build_tree}bin/\${x_conf}:\${build_dir}/\${build_tree}lib/\${x_conf}:\${build_dir}/dll/bin/\${x_conf}:\${saved_path}"
   export PATH

   # Export bin and lib pathes
   CFG_BIN="\${build_dir}/\${build_tree}bin/\${x_conf}"
   CFG_LIB="\${build_dir}/\${build_tree}lib/\${x_conf}"
   export CFG_BIN CFG_LIB

EOF

#//////////////////////////////////////////////////////////////////////////


# Read list with tests and write commands to script file.
# Also copy necessary files to the test build directory.


# Read list with tests
x_tests=`cat "$x_list" | sed -e 's/ /%gj_s4%/g'`
x_test_prev=""

# For all tests
for x_row in $x_tests; do
#echo $x_row
   # Get one row from list
   x_row=`echo "$x_row" | sed -e 's/%gj_s4%/ /g' -e 's/^ *//' -e 's/ ____ /~/g'`

   # Split it to parts
   x_rel_dir=`echo \$x_row | sed -e 's/~.*$//'`
   x_src_dir="$x_root_dir/src/$x_rel_dir"
   x_test=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_app=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_cmd=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_files=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_timeout=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_requires=`echo "$x_row" | sed -e 's/.*~//'`

   for build_tree in $x_build_trees; do

      build_tree="$build_tree/"

      # Copy specified files to the check tree
      if test ! -z "$x_files" ; then
         for x_conf in $x_confs; do
            x_path="$x_build_dir/$build_tree/check/$x_conf/$x_rel_dir"
            mkdir -p $x_path
            for i in $x_files ; do
               x_copy="$x_src_dir/$i"
               if test -f "$x_copy"  -o  -d "$x_copy" ; then
                  cp -prf "$x_copy" "$x_path"
               else
                  echo "Warning:  \"$x_copy\" must be file or directory!"
               fi
            done
         done
      fi
   done # for build_tree in x_build_trees

   # Generate extension for tests output file
   if test "$x_test" != "$x_test_prev" ; then 
      x_cnt=1
      x_test_out="out"
   else
      x_cnt=`expr $x_cnt + 1`
      x_test_out="out$x_cnt"
   fi
   x_test_prev="$x_test"

#//////////////////////////////////////////////////////////////////////////

   # Write test commands for current test into a shell script file
   cat >> $x_out <<EOF
   ######################################################################
   RunTest "$x_rel_dir" \\
           "$x_test" \\
           "$x_app" \\
           "$x_cmd" \\
           "$x_test_out" \\
           "$x_timeout" \\
           "$x_requires" \\
           "\$x_conf"
EOF

#//////////////////////////////////////////////////////////////////////////

done # for x_row in x_tests


# Write ending code into the script 
cat >> $x_out <<EOF
   ######################################################################

   # Restore saved PATH environment variable
   PATH="\$saved_path"

   done  # x_conf in configurations
done  # build_tree in \$build_trees


# Write result of the tests execution
echo
echo "Succeeded : \$count_ok"
echo "Failed    : \$count_err"
echo "Absent    : \$count_absent"
echo

if test \$count_err -eq 0; then
   echo
   echo "******** ALL TESTS COMPLETED SUCCESSFULLY ********"
   echo
fi

exit \$count_err
EOF

# Set execute mode to script
chmod a+x "$x_out"

exit 0
