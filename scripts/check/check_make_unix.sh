#! /bin/sh

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Compile a check script and copy necessary files to run tests in the 
# UNIX build tree.
#
# Usage:
#    check_make_unix.sh <in_test_list> <out_check_script> <build_dir>
#
#    in_test_list     - list of tests (it build with "make check_r")
#                       (default: "<build_dir>/check.sh.list")
#    out_check_script - name of the output test script
#                       (default: "<build_dir>/check.sh")
#    build_dir        - path to UNIX build tree like".../build/..."
#                       (default: will try determine path from current work
#                       directory -- root of build tree ) 
#
#    If any parameter is skipped that will be used default value for it.
#
# Note:
#    Work with UNIX build tree only (any configuration).
#
###########################################################################

# Parameters

res_out="check.sh"
res_list="$res_out.list"

# Field delimiters in the list (this symbols used directly in the "sed" command)
x_delim=" ____ "
x_delim_internal="~"  

x_list=$1
x_out=$2
x_build_dir=$3

if test ! -z "$x_build_dir"; then
   if test ! -d "$x_build_dir"; then
      echo "Build directory \"$x_build_dir\" don't exist."
      exit 1 
   fi
   # Expand path and remove trailing slash
   x_cur_dir=`pwd`
   cd $x_build_dir 
   x_build_dir=`pwd | sed -e 's/\/$//g'`
   cd $x_cur_dir 
else
   # Get build dir name from current path
   x_build_dir=`pwd | sed -e 's%/build.*$%%'`/build
fi

x_conf_dir=`dirname "$x_build_dir"`
x_root_dir=`dirname "$x_conf_dir"`

if test -z "$x_list"; then
   x_list="$x_build_dir/$res_list"
fi

if test -z "$x_out"; then
   x_out="$x_build_dir/$res_out"
fi

x_script_name=`echo "$x_out" | sed -e 's%^.*/%%'`

# Check list
if test ! -f "$x_list"; then
   echo "Check list file \"$x_list\" not found."
   exit 1 
fi

#//////////////////////////////////////////////////////////////////////////

cat > $x_out <<EOF
#! /bin/sh

res_journal="$x_out.journal"
res_log="$x_out.log"
res_list="$x_list"
res_concat="$x_out.out"


##  Printout USAGE info and exit

Usage() {
   cat <<EOF_usage

USAGE:  $x_script_name {run | clean | concat | concat_err}

 run         Run the tests. Create output file ("*.test_out") for each tests, 
             plus journal and log files. 
 clean       Remove all files created during the last "run" and this script 
             itself.
 concat      Concatenate all files created during the last "run" into one big 
             file "\$res_log".
 concat_err  Like previous. But into the file "\$res_concat" 
             will be added outputs of failed tests only.

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
      x_files=\`cat \$res_journal | sed -e 's/ /%gj_s4%/g'\`
      for x_file in \$x_files; do
         x_file=\`echo "\$x_file" | sed -e 's/%gj_s4%/ /g'\`
         rm -f \$x_file > /dev/null
      done
      rm -f \$res_journal \$res_log \$res_list \$res_concat > /dev/null
      rm -f $x_out > /dev/null
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
      rm -f "\$res_concat"
      ( 
      cat \$res_log | grep 'ERR \['
      x_files=\`cat \$res_journal | sed -e 's/ /%gj_s4%/g'\`
      for x_file in \$x_files; do
         x_file=\`echo "\$x_file" | sed -e 's/%gj_s4%/ /g'\`
         x_code=\`cat \$x_file | grep -c '@@@ EXIT CODE:'\`
         test \$x_code -ne 0 || continue
         x_good=\`cat \$x_file | grep -c '@@@ EXIT CODE: 0'\`
         if test \$x_good -ne 1 ; then
            echo 
            echo 
            cat \$x_file
         fi
      done
      ) >> \$res_concat
      exit 0
      ;;
#----------------------------------------------------------
   * )
      Usage "Invalid method name."
      ;;
esac

# Add current directory to PATH
PATH=".:\$PATH"
export PATH

EOF

if test -n "$x_conf_dir"  -a  -d "$x_conf_dir/lib";  then
   cat >> $x_out <<EOF
# Adjust PATH and LD_LIBRARY_PATH for running tests
PATH=".:\$PATH"
if test -n "\$LD_LIBRARY_PATH"; then
   LD_LIBRARY_PATH="$x_conf_dir/lib:\$LD_LIBRARY_PATH"
else
   LD_LIBRARY_PATH="$x_conf_dir/lib"
fi
export LD_LIBRARY_PATH
EOF
else
   echo "WARNING:  Cannot find path to the library dir."
fi

   cat >> $x_out <<EOF

# Run
count_ok=0
count_err=0
count_absent=0

rm -f "\$res_journal"
rm -f "\$res_log"

##  Run one test

RunTest() {
   # Parameters
   x_work_dir="\$1"
   x_test="\$2"
   x_app="\$3"
   x_run="\$4"
   x_test_out="\$x_work_dir/\$x_test.\$5"
   x_timeout="\$6"

   # Check existence of the test's application directory
   if test -d "\$x_work_dir"; then

      # Write header to output file 
      echo "\$x_test_out" >> \$res_journal
      (
        echo "======================================================================"
        echo "\$x_run"
        echo "======================================================================"
        echo 
      ) > \$x_test_out 2>&1

      # Remove old core file if it exist (for clarity of the test)
      rm -f "\$x_work_dir/core" > /dev/null

      # Goto the test's directory 
      cd "\$x_work_dir"
      x_cmd=\`echo "\$x_work_dir/\$x_run" | sed 's%^.*/build/%%'\`

      # And run test if it exist
      if test -f "\$x_app"; then
         # Run check
		 check_exec="$x_root_dir/scripts/check/check_exec.sh"
         \$check_exec "\$x_timeout" "\$x_run" >> \$x_test_out 2>&1
         result=\$?

         # Write result of the test into the his output file
         echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" >> \$x_test_out
         echo "@@@ EXIT CODE: \$result" >> \$x_test_out
         if test -f "\$x_work_dir/core"; then
            echo "@@@ CORE DUMPED" >> \$x_test_out
            rm -f "\$x_work_dir/core"
         fi

         # And write result also on the screen and into the log
         if test \$result -eq 0; then
            echo "OK  --  \$x_cmd"
            echo "OK  --  \$x_cmd" >> \$res_log
            count_ok=\`expr \$count_ok + 1\`
         else
            echo "ERR --  \$x_cmd"
            echo "ERR [\$result] --  \$x_cmd" >> \$res_log
            count_err=\`expr \$count_err + 1\`
         fi
      else
         echo "ABS --  \$x_cmd"
         echo "ABS --  \$x_cmd" >> \$res_log
         count_absent=\`expr \$count_absent + 1\`
      fi
  else
      # Test application is absent
      echo "ABS -- \$x_work_dir - \$x_test"
      echo "ABS -- \$x_work_dir - \$x_test" >> \$res_log
      count_absent=\`expr \$count_absent + 1\`
  fi
}

EOF

#//////////////////////////////////////////////////////////////////////////


# Read list with tests
x_tests=`cat "$x_list" | sed -e 's/ /%gj_s4%/g'`
x_test_prev=""

# For all tests
for x_row in $x_tests; do
   # Get one row from list
   x_row=`echo "$x_row" | sed -e 's/%gj_s4%/ /g' -e 's/^ *//' -e 's/\"/\\"/g' -e 's/ ____ /~/g'`

   # Split it to parts
   x_src_dir="$x_root_dir/src/`echo \"$x_row\" | sed -e 's/~.*$//'`"
   x_test=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_app=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_cmd=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_files=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_timeout=`echo "$x_row" | sed -e 's/.*~//'`

   # Application base build directory
   x_work_dir="$x_build_dir/`echo \"$x_row\" | sed -e 's/~.*$//'`"

   # Copy specified files to the build directory
   if test ! -z "$x_files" ; then
      for i in $x_files ; do
         x_copy="$x_src_dir/$i"
         if test -f "$x_copy"  -o  -d "$x_copy" ; then
            cp -prf "$x_copy" "$x_work_dir"
         else
            echo "Warning:  \"$x_copy\" must be file or directory!"
            exit 0
         fi
      done
   fi

   # Generate extension for tests output file
   if test "$x_test" != "$x_test_prev" ; then 
      x_cnt=1
      x_test_out="test_out"
   else
      x_cnt=`expr $x_cnt + 1`
      x_test_out="test_out$x_cnt"
   fi
   x_test_prev="$x_test"

#//////////////////////////////////////////////////////////////////////////

   # Write test commands for current test into a shell script file
   cat >> $x_out <<EOF
######################################################################
RunTest "$x_work_dir" \\
        "$x_test" \\
        "$x_app" \\
        "$x_cmd" \\
        "$x_test_out" \\
        "$x_timeout"
EOF

#//////////////////////////////////////////////////////////////////////////

done # for x_row in x_tests


# Write ending code into the script 
cat >> $x_out <<EOF


# Write result of the tests execution
echo
echo "Succeded : \$count_ok"
echo "Failed   : \$count_err"
echo "Absent   : \$count_absent"
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
