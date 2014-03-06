#! /bin/sh

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Compile a check script and copy necessary files to run tests in the 
# MS VisualC++ / XCODE  build tree.
#
# Usage:
#    check_make_cfg.sh <compiler> <action> <solution> <static|dll> <cfg> [build_dir]
#
#    <compiler>    - { MSVC | XCODE } 
#    <action>      - { init | create } 
#                      init   - initialize master script directory (MSVC only);
#                      create - create check script.
#    <solution>    - solution file name without .sln extention
#                    (relative path from build directory).
#    <static|dll>  - type of used libraries (static, dll).
#    <cfg>         - configuration name
#                    (Debug, DebugDLL, DebugMT, Release, ReleaseDLL, ReleaseMT,...).
#    [build_dir]   - path to build tree like ".../msvc1000_prj"
#                    (default: will try determine path from current work
#                    directory -- root of build tree) 
#
###########################################################################


# Field delimiters in the list (this symbols used directly in the "sed" command)
x_tmp="/tmp"
x_date_format="%m/%d/%Y %H:%M:%S"

# Arguments
x_compiler=$1
x_action=$2
x_solution=$3
x_libdll=$4
x_cfg=$5
x_build_dir=$6


is_msvc=false
case "$x_compiler" in
   MSVC  ) is_msvc=true ;;
   XCODE ) ;;
   * ) echo "Unknown compiler name \"$x_compiler\"."; exit 1 ;;
esac

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

### Init

if test $# -eq 2  -a  $x_action = "init"; then
   # MSVC only
   test $x_compiler != "MSVC"  &&  exit 0;
   # Change script's command interpreter from /bin/sh to /bin/bash.
   # Cygwin's shell don't works correctly with process pids.
   # echo "Changing scripts command interpreter..."
   script_dirs="scripts"
   tmp="$x_tmp/check_make_win.$$"
    
   for d in $script_dirs; do
       script_list=`find $x_root_dir/$d -name '*.sh'`
       for s in $script_list; do
           echo $s | grep 'check_make_' > /dev/null 2>&1  &&  continue
           grep '^#! */bin/sh' $s > /dev/null 2>&1
           if test $? -eq 0; then
              cp -fp $s $tmp   ||  exit 2
              sed -e 's|^#! */bin/sh.*$|#! /bin/bash|' $s > $tmp  ||  exit 2
              chmod a+x $s
              touch -r $s $tmp ||  exit 2
              cp -fp $tmp $s   ||  exit 2
              rm -f $tmp       ||  exit 2
           fi
       done
   done
   exit 0;
fi


### Create

if test $# -lt 4; then
   echo "Usage: $0 <cmd> <solution> <static|dll> <32|64> <cfg> [build_dir]"  &&  exit 1
fi

# Check list

case "$x_compiler" in
   MSVC  ) x_check_dir="$x_build_dir/$x_libdll/build/${x_solution}.check/$x_cfg" 
           x_time="time.exe -p"
           ;;
   XCODE ) x_check_dir="$x_build_dir/$x_libdll/${x_solution}.check/$x_cfg"
           x_time="time -p"
           ;;
esac
if test ! -d "$x_check_dir"; then
   echo "Check directory \"$x_check_dir\" not found."
   exit 1 
fi
x_list="$x_check_dir/check.sh.list"
if test ! -f "$x_list"; then
   echo "Check list file \"$x_list\" not found."
   exit 1 
fi
x_out="$x_check_dir/check.sh"
x_script_name=`echo "$x_out" | sed -e 's%^.*/%%'`


# Determine signature of the build (only for automatic builds)
signature=""
if test -n "$NCBI_AUTOMATED_BUILD"; then
   if test $x_compiler = "XCODE"; then
      ncbiconf="$x_build_dir/$x_libdll/inc/$x_cfg/common/config/ncbiconf_xcode_site.h"
      signature=`grep "\"XCODE.*\"" $ncbiconf | sed 's/.*\"\(XCODE.*\)\".*/\1/'`
   else
      case "$COMPILER" in
         msvc7  ) signature="MSVC_710"  ;;
         msvc8  ) signature="MSVC_800"  ;;
         msvc9  ) signature="MSVC_900"  ;;
         msvc10 ) signature="MSVC_1000" ;;
         vs2012 ) signature="VS_2012"   ;;
      esac
      signature="$signature-${x_cfg}"
      case "$x_cfg" in
         *DLL) signature="${signature}MT" ;;
         *MT ) ;;
         *   ) signature="${signature}ST" ;;
      esac
      signature="${signature}${x_libdll}${ARCH}--i386-pc-win${ARCH:-32}-${SRV_NAME}"
   fi
fi


# Check timeout multiplier (increase default check timeout in x times)
x_check_mult=1
if test $x_compiler == "MSVC" ; then
    case "$x_cfg" in
       Debug* )
           x_check_mult=3
           ;;        
    esac
fi


# Clean up check directory
(cd "$x_check_dir"  &&  find "$x_check_dir" -maxdepth 1 -mindepth 1 -type d -print | xargs rm -rf)

# Features detection
fs=`cat "${x_build_dir}/${x_libdll}/${x_cfg}/features_and_packages.txt" | tr '\r' ' '`
x_features=" "
for f in $fs; do
   x_features="$x_features$f "
done


#//////////////////////////////////////////////////////////////////////////

cat > $x_out <<EOF
#! /bin/bash

root_dir="$x_root_dir"
build_dir="$x_build_dir"
check_dir="$x_check_dir"
src_dir="\$root_dir/src"
build_tree="$x_libdll" 
build_cfg="$x_cfg" 
signature="$signature"

res_script="$x_out"
res_journal="\$res_script.journal"
res_log="\$res_script.log"
res_list="\$res_script.list"
res_concat="\$res_script.out"
res_concat_err="\$res_script.out_err"

is_db_load=false


##  Printout USAGE info and exit

Usage() {
   cat <<EOF_usage

USAGE:  $x_script_name {run | concat | concat_err}

 run         Run tests. Create output file ("*.out") for each test, 
             journal and log files. 
 concat      Concatenate all output files created during the last "run" 
             into one file "\$res_log".
 concat_err  Like 'concat', but output file "\$res_concat_err" 
             will contains outputs of failed tests only.

ERROR:  \$1
EOF_usage
# Undocumented command:
#     load_to_db  Load data about test to database keeping all statistics.

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
      # See RunTest() below
      ;;
#----------------------------------------------------------
   concat )
      rm -f "\$res_concat"
      ( 
#      cat \$res_log
      test -f \$res_journal  ||  exit 0
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
#      egrep 'ERR \[|TO  -' \$res_log
      test -f \$res_journal  ||  exit 0
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
   load_to_db )
      is_db_load=true
      # See RunTest() below
      ;;
#----------------------------------------------------------
   * )
      Usage "Invalid method name."
      ;;
esac


## Run


# Set log_site for tests
export NCBI_APPLOG_SITE=testcxx

# Export some global vars
export top_srcdir="\$root_dir"

# Add current, build and scripts directories to PATH
export PATH=".:\${build_dir}:\${root_dir}/scripts/common/impl::\${root_dir}/scripts/common/check:\${PATH}"

# Define time-guard script to run tests from other scripts
export CHECK_EXEC="\${root_dir}/scripts/common/check/check_exec_test.sh"
export CHECK_EXEC_STDIN="\$CHECK_EXEC -stdin"

# Define signature of the current build to use by test applications
export CHECK_SIGNATURE="\$signature"

# Enable silent abort for NCBI applications on fatal errors
export DIAG_SILENT_ABORT="Y"

# Use AppLog-style output format in the testsuite by default
test -z "\$DIAG_OLD_POST_FORMAT"  &&  export DIAG_OLD_POST_FORMAT=false

# Default check tools
if test -z "\$NCBI_CHECK_TOOLS"; then
    NCBI_CHECK_TOOLS="regular"
fi

# Check timeout multiplier (increase default check timeout in x times)
if test -z "\$NCBI_CHECK_TIMEOUT_MULT"; then
    NCBI_CHECK_TIMEOUT_MULT=$x_check_mult
fi

# Path to test data, used by some scripts and applications
if test -z "\$NCBI_TEST_DATA"; then
    case `uname -s` in
       CYGWIN* ) NCBI_TEST_DATA=//snowman/win-coremake/Scripts/test_data ;;
       *)        NCBI_TEST_DATA=/am/ncbiapdata/test_data ;;
    esac
    export NCBI_TEST_DATA
fi

count_ok=0
count_err=0
count_timeout=0
count_absent=0
configurations="$x_confs"

if \$is_db_load; then
    echo "--------------------------------------------------" >> "\$build_dir/test_stat_load.log" 2>&1
    echo "Loading tests from: \$check_dir"                    >> "\$build_dir/test_stat_load.log" 2>&1
    echo "--------------------------------------------------" >> "\$build_dir/test_stat_load.log" 2>&1
else
    rm -f "\$res_journal"
    rm -f "\$res_log"
fi


##  Run one test

RunTest() {
    # Parameters
    x_wdir="\$1"
    x_test="\$2"
    x_app="\$3"
    x_run="\${4:-\$x_app}"
    x_real_name="\$5"
    x_name="\${5:-\$x_run}"  
    x_ext="\$6"
    x_timeout="\$7"
    x_authors="\$8"
 
    x_work_dir="\$check_dir/\$x_wdir"
    mkdir -p "\$x_work_dir" > /dev/null 2>&1 

    # Determine test application name
    x_path_run="\$build_dir/\${build_tree}/bin/\$build_cfg"
    result=1
    x_path_app="\$x_path_run/\$x_app"
    if test ! -f "\$x_path_app"; then
       x_path_app="\$x_path_run/\$x_test"
       if test ! -f "\$x_path_app"; then
          result=0
       fi
    fi

    # Run test under all specified check tools   
    for tool in \$NCBI_CHECK_TOOLS; do

        tool_lo=\`echo \$tool | tr '[A-Z]' '[a-z]'\`
        tool_up=\`echo \$tool | tr '[a-z]' '[A-Z]'\`
        
        case "\$tool_lo" in
           regular | inspxe ) ;;
                          * ) continue ;;
        esac
        if test \$tool_lo = "regular"; then
           x_cmd="[\$build_tree/\$build_cfg/\$x_wdir] \$x_name"
           x_test_out="\$x_work_dir/\$x_app.test_out\$x_ext"
           x_test_rep="\$x_work_dir/\$x_app.test_rep\$x_ext"
           x_boost_rep="\$x_work_dir/\$x_app.boost_rep\$x_ext"
           x_applog_sh="\$x_work_dir/\$x_app.applog.sh"
        else
           x_cmd="[\$build_tree/\$build_cfg/\$x_wdir] \$tool_up \$x_name"
           x_test_out="\$x_work_dir/\$x_app.test_out\$x_ext.\$tool_lo"
           x_test_rep="\$x_work_dir/\$x_app.test_rep\$x_ext.\$tool_lo"
           x_boost_rep="\$x_work_dir/\$x_app.boost_rep\$x_ext.\$tool_lo"
           x_applog_sh="\$x_work_dir/\$x_app.applog.\$tool_lo.sh"
        fi

        if \$is_db_load; then
           echo "\$x_path_app:" >> "\$build_dir/test_stat_load.log" 2>&1
           case "$x_compiler" in
             MSVC )
                test_stat_load "\$(cygpath -w "\$x_test_rep")" "\$(cygpath -w "\$x_test_out")" "\$(cygpath -w "\$x_boost_rep")" "\$(cygpath -w "\$top_srcdir/build_info")" "\$(cygpath -w "\$x_applog_sh")" >> "\$build_dir/test_stat_load.log" 2>&1
                ;;        
             XCODE ) 
                $NCBI/bin/_production/CPPCORE/test_stat_load "\$x_test_rep" "\$x_test_out" "\$x_boost_rep" "\$top_srcdir/build_info" >> "\$build_dir/test_stat_load.log" 2>&1
                ;;
           esac
           if test -f \$x_applog_sh; then
              chmod a+x \$x_applog_sh
              ##\$x_applog_sh >> "\$build_dir/test_stat_load_applog.log" 2>&1
           fi
           continue
        fi
   
        if test -n "\$NCBI_AUTOMATED_BUILD"; then
           echo "\$signature"   >> "\$x_test_rep"
           echo "\$x_wdir"      >> "\$x_test_rep"
           echo "\$x_run"       >> "\$x_test_rep"
           echo "\$x_real_name" >> "\$x_test_rep"
           case "$x_compiler" in
              MSVC )
                 export NCBI_BOOST_REPORT_FILE="\$(cygpath -w "\$x_boost_rep")"
                 ;;        
              XCODE ) 
                 export NCBI_BOOST_REPORT_FILE="\$x_boost_rep"
                 ;;
           esac
        fi

        # Check existence of the test application
        if test \$result -eq 1; then

           # Goto the work directory 
           cd "\$x_work_dir"

           # Fix empty parameters (replace "" to \"\", '' to \'\')
           x_run_fix=\`echo "\$x_run" | sed -e 's/""/\\\\\\\\\\"\\\\\\\\\\"/g' -e "s/''/\\\\\\\\\\'\\\\\\\\\\'/g"\`
           # Fix empty parameters (put each in '' or "")
           x_run_fix=\`echo "\$x_run" | sed -e 's/""/'"'&'/g" -e "s/''/\\\\'\\\\'/g"\`

           # Define check tool variables
           case "\$tool_lo" in
           regular  ) ;;
           inspxe   ) NCBI_CHECK_TOOL="inspxe.sh" 
                      NCBI_CHECK_TIMEOUT_MULT=20
                      ;;
           esac
           export NCBI_CHECK_TOOL
           CHECK_TIMEOUT=\`expr \$x_timeout \* \$NCBI_CHECK_TIMEOUT_MULT\`
           export CHECK_TIMEOUT

           echo \$x_run | grep '\.sh' > /dev/null 2>&1 
           if test \$? -eq 0;  then
              # Run script without any check tools.
              # It will be applied inside script using \$CHECK_EXEC.
              xx_run="\$x_run_fix"
           else
              # Run under check tool
              xx_run="\$NCBI_CHECK_TOOL \$x_run_fix"
           fi

           # Write header to output file 
           echo "\$x_test_out" >> \$res_journal
           (
              echo "======================================================================"
              echo "\$build_tree/\$build_cfg - \$x_name"
              echo "======================================================================"
              echo 
           ) > \$x_test_out 2>&1


           # Run check
           rm -f check_exec.pid > /dev/null 2>&1
           start_time="\`date +'$x_date_format'\`"
           check_exec="\$root_dir/scripts/common/check/check_exec.sh"
           \$check_exec $x_time \`eval echo \$xx_run\` > \$x_test_out.\$\$ 2>&1
           result=\$?
           stop_time="\`date +'$x_date_format'\`"
           load_avg="\`uptime | sed -e 's/.*averages*: *\(.*\) *$/\1/' -e 's/[, ][, ]*/ /g'\`"

           sed -e '/ ["][$][@]["].*\$/ {
               s/^.*: //
               s/ ["][$][@]["].*$//
            }' \$x_test_out.\$\$ >> \$x_test_out

           # RunID
           runpid='?'
           test -f check_exec.pid  &&  runpid="\`cat check_exec.pid\`"
           runid="\`date -u +%y%m%d%H%M%S\`-\$runpid-\`uname -n\`"
           runid="\`echo \$runid | tr -d '\n\r'\`"
           rm -f check_exec.pid > /dev/null 2>&1
                
           # Get application execution time
           exec_time=\`tail -3 \$x_test_out.\$\$\`
           exec_time=\`echo "\$exec_time" | tr '\n\r' '%%'\`
           echo \$exec_time | grep 'real [0-9]\|Maximum execution .* is exceeded' > /dev/null 2>&1 
           if test \$? -eq 0;  then
              exec_time=\`echo \$exec_time | sed -e 's/^%*//' -e 's/%*$//' -e 's/%%/%/g' -e 's/%/, /g' -e 's/[ ] */ /g'\`
           else
              exec_time='unparsable timing stats'
           fi
           rm -f $x_tmp/\$\$.out
           rm -f \$x_test_out.\$\$

           # Write result of the test into the his output file
           echo "Start time   : \$start_time"   >> \$x_test_out
           echo "Stop time    : \$stop_time"    >> \$x_test_out
           echo "Load averages: \$load_avg"     >> \$x_test_out
           echo >> \$x_test_out
           echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" >> \$x_test_out
           echo "@@@ EXIT CODE: \$result" >> \$x_test_out

           # And write result also on the screen and into the log
           if grep -q NCBI_UNITTEST_DISABLED \$x_test_out >/dev/null; then
              echo "DIS --  \$x_cmd"
              echo "DIS --  \$x_cmd" >> \$res_log
              count_absent=\`expr \$count_absent + 1\`
              test -n "\$NCBI_AUTOMATED_BUILD" && echo "DIS" >> "\$x_test_rep"
      
           elif grep NCBI_UNITTEST_SKIPPED \$x_test_out >/dev/null; then
              echo "SKP --  \$x_cmd"
              echo "SKP --  \$x_cmd" >> \$res_log
              count_absent=\`expr \$count_absent + 1\`
              test -n "\$NCBI_AUTOMATED_BUILD" && echo "SKP" >> "\$x_test_rep"
      
           elif grep NCBI_UNITTEST_TIMEOUTS_BUT_NO_ERRORS \$x_test_out >/dev/null; then
              echo "TO  --  \$x_cmd"
              echo "TO  --  \$x_cmd" >> \$res_log
              count_timeout=\`expr \$count_timeout + 1\`
              test -n "\$NCBI_AUTOMATED_BUILD" && echo "TO" >> "\$x_test_rep"

           elif egrep "Maximum execution .* is exceeded" \$x_test_out >/dev/null; then
              echo "TO  --  \$x_cmd     (\$exec_time)"
              echo "TO  --  \$x_cmd     (\$exec_time)" >> \$res_log
              count_timeout=\`expr \$count_timeout + 1\`
              test -n "\$NCBI_AUTOMATED_BUILD" && echo "TO" >> "\$x_test_rep"

           elif test \$result -eq 0; then
              echo "OK  --  \$x_cmd     (\$exec_time)"
              echo "OK  --  \$x_cmd     (\$exec_time)" >> \$res_log
              count_ok=\`expr \$count_ok + 1\`
              test -n "\$NCBI_AUTOMATED_BUILD" && echo "OK" >> "\$x_test_rep"

           else
               echo "ERR [\$result] --  \$x_cmd     (\$exec_time)"
               echo "ERR [\$result] --  \$x_cmd     (\$exec_time)" >> \$res_log
               count_err=\`expr \$count_err + 1\`
               test -n "\$NCBI_AUTOMATED_BUILD" && echo "ERR" >> "\$x_test_rep"
           fi
   
           if test -n "\$NCBI_AUTOMATED_BUILD"; then
              echo "\$start_time" >> "\$x_test_rep"
              echo "\$result"     >> "\$x_test_rep"
              echo "\$exec_time"  >> "\$x_test_rep"
              echo "\$x_authors"  >> "\$x_test_rep"
              echo "\$load_avg"   >> "\$x_test_rep"
              echo "\$runid"      >> "\$x_test_rep"
           fi

        else  # Check existence of the test's application directory
           # Test application is absent
           echo "ABS --  \$x_cmd - \$x_test"
           echo "ABS --  \$x_cmd - \$x_test" >> \$res_log
           count_absent=\`expr \$count_absent + 1\`
           if test -n "\$NCBI_AUTOMATED_BUILD"; then
              echo "ABS" >> "\$x_test_rep"
              echo "\`date +'$x_date_format'\`" >> "\$x_test_rep"
           fi
        fi
        
    done  # Run test under all specified check tools   
}


# Save value of PATH environment variable
saved_path="\$PATH"

# Features detection
FEATURES="$x_features"
export FEATURES

# Add current configuration's build 'bin' and 'lib' directories to PATH
export PATH=".:\${build_dir}/\${build_tree}/bin/\${build_cfg}:\${build_dir}/\${build_tree}/lib/\${build_cfg}:\${saved_path}"

# Export bin and lib paths
export CFG_BIN="\${build_dir}/\${build_tree}/bin/\${build_cfg}"
export CFG_LIB="\${build_dir}/\${build_tree}/lib/\${build_cfg}"


EOF

#//////////////////////////////////////////////////////////////////////////


# Read list with tests and write commands to script file.
# Also copy necessary files to the test build directory.


# Read list with tests
x_tests=`cat "$x_list" | sed -e 's/ /%gj_s4%/g'`
x_test_prev=""
   
# For all tests
for x_row in $x_tests; do
   # Get one row from list
   x_row=`echo "$x_row" | sed -e 's/%gj_s4%/ /g' -e 's/^ *//' -e 's/ ____ /~/g'`

   # Split it to parts
   x_rel_dir=`echo \$x_row | sed -e 's/~.*$//'`
   x_src_dir="$x_root_dir/src/$x_rel_dir"
   x_test=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_app=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_cmd=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//' -e 's/\"/\\\\"/g'`
   x_name=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_files=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_timeout=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   ###x_requires=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_authors=`echo "$x_row" | sed -e 's/.*~//'`

   # Check application requirements
   # TODO:
   #    This check can be removed later, when project_tree_builder starts to check requiments
   #    on check list generation step.
   ###for x_req in $x_requires; do
   ###   (echo "$x_features" | grep " $x_req " > /dev/null)  ||  continue 2
   ###done
   
   # Copy specified files into the check tree
   if test ! -z "$x_files" ; then
      x_path="$x_check_dir/$x_rel_dir"
      mkdir -p "$x_path"
      for i in $x_files ; do
         x_copy="$x_src_dir/$i"
         if test -f "$x_copy"  -o  -d "$x_copy" ; then
            if $is_msvc; then
               # Do not preserve permissions, it is not always possible for NTFS
               cp -rf "$x_copy" "$x_path"
            else
               cp -prf "$x_copy" "$x_path"
            fi
            find "$x_path/$i" -name .svn -print 2> /dev/null | xargs rm -rf
         else
            echo "Warning:  \"$x_copy\" must be file or directory!"
         fi
      done
   fi
   
   # Generate extension for tests output file
   if test "$x_test" != "$x_test_prev" ; then 
      x_cnt=1
      x_test_ext=""
   else
      x_cnt=`expr $x_cnt + 1`
      x_test_ext="$x_cnt"
   fi
   x_test_prev="$x_test"

   # Write current test commands into a script file
   cat >> $x_out <<EOF
######################################################################
RunTest "$x_rel_dir"  \\
        "$x_test"     \\
        "$x_app"      \\
        "$x_cmd"      \\
        "$x_name"     \\
        "$x_test_ext" \\
        "$x_timeout"  \\
        "$x_authors"
EOF

#//////////////////////////////////////////////////////////////////////////

done # for x_row in x_tests


# Write ending code into the script 
cat >> $x_out <<EOF
######################################################################


# Restore saved PATH environment variable
PATH="\$saved_path"

# Write result of the tests execution
if ! \$is_db_load; then
   echo
   echo "Succeeded : \$count_ok"
   echo "Timeout   : \$count_timeout"
   echo "Failed    : \$count_err"
   echo "Absent    : \$count_absent"
   echo
fi

exit \$count_err
EOF

# Set execute mode to script
chmod a+x "$x_out"

exit 0
