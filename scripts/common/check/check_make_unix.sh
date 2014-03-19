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
#    check_make_unix.sh <test_list> <build_dir> <target_dir> <check_script>
#
#    test_list       - a list of tests (it build with "make check_r")
#                      (default: "<build_dir>/check.sh.list")
#    signature       - build signature
#    build_dir       - path to UNIX build tree like".../build/..."
#                      (default: will try determine path from current work
#                      directory -- root of build tree ) 
#    top_srcdir      - path to the root src directory
#                      (default: will try determine path from current work
#                      directory -- root of build tree ) 
#    target_dir      - path where the check script and logs will be created
#                      (default: current dir) 
#    check_script    - name of the check script (without path).
#                      (default: "check.sh" / "<target_dir>/check.sh")
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

# Fields delimiters in the list
# (this symbols used directly in the "sed" command)
x_delim=" ____ "
x_delim_internal="~"
x_tmp="/var/tmp"

x_date_format="%m/%d/%Y %H:%M:%S"

x_list=$1
x_signature=$2
x_build_dir=$3
x_top_srcdir=$4
x_target_dir=$5
x_out=$6

# Detect Cygwin
case `uname -s` in
   CYGWIN* ) cygwin=true  ;;
   *)        cygwin=false ;;
esac

# Check for build dir
if test ! -z "$x_build_dir"; then
   if test ! -d "$x_build_dir"; then
      echo "Build directory \"$x_build_dir\" don't exist."
      exit 1 
   fi
   x_build_dir=`(cd "$x_build_dir"; pwd | sed -e 's/\/$//g')`
else
   # Get build dir name from the current path
   x_build_dir=`pwd | sed -e 's%/build.*$%%'`
   if test -d "$x_build_dir/build"; then
      x_build_dir="$x_build_dir/build"
   fi
fi

x_conf_dir=`dirname "$x_build_dir"`
x_bin_dir=`(cd "$x_build_dir/../bin"; pwd | sed -e 's/\/$//g')`

# Check for top_srcdir
if test ! -z "$x_top_srcdir"; then
   if test ! -d "$x_top_srcdir"; then
      echo "Top source directory \"$x_top_srcdir\" don't exist."
      exit 1 
   fi
   x_root_dir=`(cd "$x_top_srcdir"; pwd | sed -e 's/\/$//g')`
else
   # Get top src dir name from the script directory
   x_check_scripts_dir=`dirname "$0"`
   x_scripts_dir=`dirname "$x_check_scripts_dir"`
   x_root_dir=`dirname "$x_scripts_dir"`
fi

# Check for target dir
if test ! -z "$x_target_dir"; then
   if test ! -d "$x_target_dir"; then
      echo "Target directory \"$x_target_dir\" don't exist."
      exit 1 
   fi
    x_target_dir=`(cd "$x_target_dir"; pwd | sed -e 's/\/$//g')`
else
   x_target_dir=`pwd`
fi

# Check for a imported project or intree project
if test -f Makefile.out ; then
   x_import_prj="yes"
   x_import_root=`sed -ne 's/^import_root *= *//p' Makefile`
   # x_compile_dir="`pwd | sed -e 's%/internal/c++/src.*$%%g'`/internal/c++/src"
   x_compile_dir=`cd $x_import_root; pwd`
else
   x_import_prj="no"
   x_compile_dir="$x_build_dir"
fi

if test -z "$x_list"; then
   x_list="$x_target_dir/$res_list"
fi

if test -z "$x_out"; then
   x_out="$x_target_dir/$res_out"
fi

x_script_name=`echo "$x_out" | sed -e 's%^.*/%%'`

# Check for a list file
if test ! -f "$x_list"; then
   echo "Check list file \"$x_list\" not found."
   exit 1 
fi

# Features detection
x_features=""
for f in $x_conf_dir/status/*.enabled; do
   f=`echo $f | sed 's|^.*/status/\(.*\).enabled$|\1|g'`
   x_features="$x_features$f "
done


#echo ----------------------------------------------------------------------
#echo "Imported project  :" $x_import_prj
#echo "C++ root dir      :" $x_root_dir
#echo "Configuration dir :" $x_conf_dir
#echo "Build dir         :" $x_build_dir
#echo "Compile dir       :" $x_compile_dir
#echo "Target dir        :" $x_target_dir
#echo "Check script      :" $x_out
#echo ----------------------------------------------------------------------

#//////////////////////////////////////////////////////////////////////////

cat > $x_out <<EOF
#! /bin/sh

root_dir="$x_root_dir"
build_dir="$x_build_dir"
conf_dir="$x_conf_dir"
compile_dir="$x_compile_dir"
bin_dir="$x_bin_dir"
script="$x_out"
cygwin=$cygwin

res_journal="\$script.journal"
res_log="\$script.log"
res_list="$x_list"
res_concat="\$script.out"
res_concat_err="\$script.out_err"

# Define both senses to accommodate shells lacking !
is_report_err=false
no_report_err=true
is_db_load=false
no_db_load=true
signature="$x_signature"
sendmail=''
domain='@ncbi.nlm.nih.gov'

# Include COMMON.SH
. \${root_dir}/scripts/common/common.sh


##  Printout USAGE info and exit

Usage() {
   cat <<EOF_usage

USAGE:  $x_script_name {run | clean | concat | concat_err}

 run         Run the tests. Create output file ("*.test_out") for each test, 
             plus journal and log files. 
 clean       Remove all files created during the last "run" and this script 
             itself.
 concat      Concatenate all files created during the last "run" into one big 
             file "\$res_log".
 concat_err  Like previous. But into the file "\$res_concat_err" 
             will be added outputs of failed tests only.

ERROR:  \$1
EOF_usage
# Undocumented commands:
#     report_err  Report failed tests directly to developers.
#     load_to_db  Load data about test to database keeping all statistics.

    exit 1
}

if test \$# -ne 1; then
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
   clean )
      x_files=\`cat \$res_journal | sed -e 's/ /%gj_s4%/g'\`
      for x_file in \$x_files; do
         x_file=\`echo "\$x_file" | sed -e 's/%gj_s4%/ /g'\`
         rm -f \$x_file > /dev/null
      done
      rm -f \$res_journal \$res_log \$res_list \$res_concat \$res_concat_err > /dev/null
      rm -f \$script > /dev/null
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
      cat \$res_log | egrep 'ERR \[|TO  -'
      x_files=\`cat \$res_journal | sed -e 's/ /%gj_s4%/g'\`
      for x_file in \$x_files; do
         x_file=\`echo "\$x_file" | sed -e 's/%gj_s4%/ /g'\`
         x_code=\`cat \$x_file | grep -c '@@@ EXIT CODE:'\`
         test \$x_code -ne 0 || continue
         x_good=\`cat \$x_file | grep -c '@@@ EXIT CODE: 0'\`
         if test \$x_good -ne 1; then
            echo 
            echo 
            cat \$x_file
         fi
      done
      ) >> \$res_concat_err
      exit 0
      ;;
#----------------------------------------------------------
   report_err )
      # This method works inside NCBI only 
      test "\$NCBI_CHECK_MAILTO_AUTHORS." = 'Y.'  ||  exit 0;
      if test -x /usr/sbin/sendmail; then
         sendmail="/usr/sbin/sendmail -oi"
      elif test -x /usr/lib/sendmail; then
         sendmail="/usr/lib/sendmail -oi"
      else
         echo sendmail not found on this platform
         exit 0
      fi
      is_report_err=true
      no_report_err=false
      # See RunTest() below
      ;;
#----------------------------------------------------------
   load_to_db )
      is_db_load=true
      no_db_load=false
      rm -f "$x_build_dir/test_stat_load.log"
      # See RunTest() below
      ;;
#----------------------------------------------------------
   * )
      Usage "Invalid method name."
      ;;
esac


# Set log_site for tests
NCBI_APPLOG_SITE=testcxx
export NCBI_APPLOG_SITE

# Include configuration file
. \${build_dir}/check.cfg
if test -z "\$NCBI_CHECK_TOOLS"; then
   NCBI_CHECK_TOOLS="regular"
fi
# Check timeout multiplier (increase default check timeout in x times)
if test -z "\$NCBI_CHECK_TIMEOUT_MULT"; then
   NCBI_CHECK_TIMEOUT_MULT=1
fi

# Path to test data, used by some scripts and applications
if test -z "\$NCBI_TEST_DATA"; then
    if [ \$cygwin = true ]; then
       NCBI_TEST_DATA=//snowman/win-coremake/Scripts/test_data
    else
       NCBI_TEST_DATA=/am/ncbiapdata/test_data
    fi
    export NCBI_TEST_DATA
fi

# Valgrind configuration
VALGRIND_SUP="\${root_dir}/scripts/common/check/valgrind.supp"
VALGRIND_CMD="--tool=memcheck --suppressions=\$VALGRIND_SUP"
if (valgrind --ncbi --help) >/dev/null 2>&1; then
    VALGRIND_CMD="--ncbi \$VALGRIND_CMD" # --ncbi must be the first option!
fi

# Export some global vars
top_srcdir="\$root_dir"
export top_srcdir
FEATURES="$x_features"
export FEATURES

# Redirect output for C++ diagnostic framework to stderr
NCBI_CONFIG__LOG__FILE="-"
export NCBI_CONFIG__LOG__FILE

# Add current, build and scripts directories to PATH
PATH=".:\${build_dir}:\${root_dir}/scripts/common/impl:\${PATH}"
export PATH

# Export bin and lib pathes
CFG_BIN="\${conf_dir}/bin"
CFG_LIB="\${conf_dir}/lib"
export CFG_BIN CFG_LIB

# Define time-guard script to run tests from other scripts
check_exec="\$root_dir/scripts/common/check/check_exec.sh"
CHECK_EXEC="\${root_dir}/scripts/common/check/check_exec_test.sh"
CHECK_EXEC_STDIN="\$CHECK_EXEC -stdin"
CHECK_SIGNATURE="\$signature"
export CHECK_EXEC
export CHECK_EXEC_STDIN
export CHECK_SIGNATURE

# Use AppLog-style output format in the testsuite by default
if test -z "\$DIAG_OLD_POST_FORMAT"; then
    DIAG_OLD_POST_FORMAT=false
    export DIAG_OLD_POST_FORMAT
fi

# Avoid possible hangs on Mac OS X.
DYLD_BIND_AT_LAUNCH=1
export DYLD_BIND_AT_LAUNCH

case " \$FEATURES " in
    *\ MaxDebug\ * )
         case "\$signature" in
	     *-linux* ) MALLOC_DEBUG_=2; export MALLOC_DEBUG_ ;;
         esac
         case "\$signature" in
             GCC* | ICC* ) NCBI_CHECK_TIMEOUT_MULT=20 ;;
         esac
         ;;
esac

EOF

if test -n "$x_conf_dir"  -a  -d "$x_conf_dir/lib";  then
   cat >> $x_out <<EOF
# Add a library path for running tests
. \$root_dir/scripts/common/common.sh
COMMON_AddRunpath "\$conf_dir/lib"
EOF
else
   echo "WARNING:  Cannot find path to the library dir."
fi
# Add additional path for imported projects to point to local /lib first
if test "$x_import_prj" = "yes"; then
    local_lib=`(cd "$x_compile_dir/../lib"; pwd | sed -e 's/\/$//g')`
    if test -n "$local_lib"  -a  -d "$local_lib";  then
   cat >> $x_out <<EOF
COMMON_AddRunpath "$local_lib"
EOF
    fi
fi


cat >> $x_out <<EOF

# Run
count_ok=0
count_err=0
count_timeout=0
count_absent=0
count_total=0

if \$no_report_err && \$no_db_load; then
   rm -f "\$res_journal"
   rm -f "\$res_log"
fi

if test "\$NCBI_CHECK_SETLIMITS" != "0"; then
   ulimit -c 1000000
   if [ \$cygwin = false ]; then
      ulimit -v 4000000
   fi
fi


##  Run one test

RunTest()
{
    # Parameters
    x_work_dir_tail="\$1"
    x_work_dir="\$compile_dir/\$x_work_dir_tail"
    x_test="\$2"
    x_app="\$3"
    x_run="\${4:-\$x_app}"
    x_real_name="\$5"
    x_name="\${5:-\$x_run}"
    x_ext="\$6"
    x_timeout="\$7"
    x_authors="\$8"

    if \$is_report_err; then
        # Authors are not defined for this test
        test -z "\$x_authors"  &&  return 0
    fi

    count_total=\`expr \$count_total + 1\`
    x_log="$x_tmp/\$\$.out\$count_total"


    # Run test under all specified check tools   
    for tool in \$NCBI_CHECK_TOOLS; do

        tool_lo=\`echo \$tool | tr '[A-Z]' '[a-z]'\`
        tool_up=\`echo \$tool | tr '[a-z]' '[A-Z]'\`
        
        case "\$tool_lo" in
            regular | valgrind ) ;;
                             * ) continue ;;
        esac
        
        x_cmd="[\$x_work_dir_tail] \$x_name"
        if test \$tool_lo = "regular"; then
            #x_cmd="[\$x_work_dir_tail] \$x_name"
            x_test_out="\$x_work_dir/\$x_test.test_out\$x_ext"
            x_test_rep="\$x_work_dir/\$x_test.test_rep\$x_ext"
            x_boost_rep="\$x_work_dir/\$x_test.boost_rep\$x_ext"
        else
            #x_cmd="[\$x_work_dir_tail] \$tool_up \$x_name"
            x_test_out="\$x_work_dir/\$x_test.test_out\$x_ext.\$tool_lo"
            x_test_rep="\$x_work_dir/\$x_test.test_rep\$x_ext.\$tool_lo"
            x_boost_rep="\$x_work_dir/\$x_test.boost_rep\$x_ext.\$tool_lo"
        fi

        if \$is_db_load; then
            case \`uname -s\` in
              CYGWIN* )
                test_stat_load "\$(cygpath -w "\$x_test_rep")" "\$(cygpath -w "\$x_test_out")" "\$(cygpath -w "\$x_boost_rep")" "\$(cygpath -w "\$top_srcdir/build_info")" >> "$x_build_dir/test_stat_load.log" 2>&1 ;;
              IRIX* )
                \$NCBI/bin/_production/CPPCORE/test_stat_load.sh "\$x_test_rep" "\$x_test_out" "\$x_boost_rep" "\$top_srcdir/build_info" >> "$x_build_dir/test_stat_load.log" 2>&1 ;;
              * )
                \$NCBI/bin/_production/CPPCORE/test_stat_load "\$x_test_rep" "\$x_test_out" "\$x_boost_rep" "\$top_srcdir/build_info" >> "$x_build_dir/test_stat_load.log" 2>&1 ;;
            esac
            continue
        fi
        
        if \$no_report_err; then
           if test -n "\$NCBI_AUTOMATED_BUILD"; then
               echo "\$signature \$NCBI_CHECK_OS_NAME" > "\$x_test_rep"
               echo "\$x_work_dir_tail" >> "\$x_test_rep"
               echo "\$x_run" >> "\$x_test_rep"
               echo "\$x_real_name" >> "\$x_test_rep"
               NCBI_BOOST_REPORT_FILE="\$x_boost_rep"
               export NCBI_BOOST_REPORT_FILE
           fi
        fi

        # Check existence of the test's application directory
        if test -d "\$x_work_dir"; then

            # Goto the test's directory 
            cd "\$x_work_dir"

            # Run test if it exist
            if test -f "\$x_app"; then

                _RLD_ARGS="-log \$x_log"
                export _RLD_ARGS

                # Fix empty parameters (replace "" to \"\", '' to \'\')
                x_run_fix=\`echo "\$x_run" | sed -e 's/""/\\\\\\\\\\"\\\\\\\\\\"/g' -e "s/''/\\\\\\\\\\'\\\\\\\\\\'/g"\`


                # Define check tool variables
                NCBI_CHECK_TOOL=\`eval echo "\$"NCBI_CHECK_\${tool_up}""\`
                case "\$tool_lo" in
                regular  ) ;;
                valgrind ) NCBI_CHECK_TOOL="\$NCBI_CHECK_TOOL \$VALGRIND_CMD" 
                           NCBI_CHECK_TIMEOUT_MULT=15
                           NCBI_RUN_UNDER_VALGRIND="yes"
                           export NCBI_RUN_UNDER_VALGRIND
                           NCBI_RUN_UNDER_CHECK_TOOL="yes"
                           export NCBI_RUN_UNDER_CHECK_TOOL
                           ;;
                esac
                export NCBI_CHECK_TOOL
                CHECK_TIMEOUT=\`expr \$x_timeout \* \$NCBI_CHECK_TIMEOUT_MULT\`
                export CHECK_TIMEOUT

                # Just need to report errors to authors?
                if \$is_report_err; then
                    test -f "\$x_test_out" || continue
                    x_code=\`cat \$x_test_out | grep -c '@@@ EXIT CODE:'\`
                    test \$x_code -ne 0 || continue
                    x_good=\`cat \$x_test_out | grep -c '@@@ EXIT CODE: 0'\`
                    if test \$x_good -eq 1; then
                        continue
                    fi
                    MailToAuthors "\$x_authors" "\$x_test_out"
                    continue
                fi
         
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
                    echo "\$x_name"
                    echo "======================================================================"
                    echo 
                    if test "\$x_run" != "\$x_name"; then
                       echo "Command line: \$x_run"
                       echo 
                    fi
                ) > \$x_test_out 2>&1

                # Remove old core file if it exist (for clarity of the test)
                corefile="\$x_work_dir/core"
                rm -f "\$corefile" > /dev/null 2>&1
                rm -f check_exec.pid > /dev/null 2>&1

                # Run check
                start_time="\`date +'$x_date_format'\`"
                        
                # Use separate shell to run test.
                # This will allow to know execution time for applications with timeout.
                # Also, process guard works better if used after "time -p".
                launch_sh="/var/tmp/launch.\$\$.sh"
cat > \$launch_sh <<EOF_launch
#! /bin/sh
exec time -p \$check_exec \`eval echo \$xx_run\`
EOF_launch
                chmod a+x \$launch_sh
                \$launch_sh >\$x_log 2>&1
                result=\$?
                stop_time="\`date +'$x_date_format'\`"
                load_avg="\`uptime | sed -e 's/.*averages*: *\(.*\) *$/\1/' -e 's/[, ][, ]*/ /g'\`"
                rm \$launch_sh

                sed -e '/ ["][$][@]["].*\$/ {
                        s/^.*: //
                        s/ ["][$][@]["].*$//
                }' \$x_log >> \$x_test_out

                # RunID
                runpid='?'
                test -f check_exec.pid  &&  runpid="\`cat check_exec.pid\`"
                runid="\`date -u +%y%m%d%H%M%S\`-\$runpid-\`uname -n\`"
                runid="\`echo \$runid | tr -d '\n\r'\`"
                rm -f check_exec.pid > /dev/null 2>&1
                
                # Get application execution time
                exec_time=\`\$build_dir/sysdep.sh tl 7 \$x_log | tr '\n\r' '%%' | tr -d '\000-\037' | tr  -d '\176-\377'\`
                echo \$exec_time | egrep 'real [0-9]|Maximum execution .* is exceeded' > /dev/null 2>&1 
                if test \$? -eq 0;  then
                    exec_time=\`echo \$exec_time |   \\
                                sed -e 's/%%/%/g'    \\
                                    -e 's/%$//'      \\
                                    -e 's/%/, /g'    \\
                                    -e 's/[ ] */ /g' \\
                                    -e 's/.*\(Maximum execution .* is exceeded\).*$/\1/' \\
                                    -e 's/^.*\(real [0-9][0-9]*[.][0-9][0-9]*\)/\1/' \\
                                    -e 's/\(sys [0-9][0-9]*[.][0-9][0-9]*\).*/\1/'\`
                else
                    exec_time='unparsable timing stats'
                fi
               
                rm -f \$x_log

                # Analize check tool output
                case "\$tool_lo" in
                    valgrind ) summary_all=\`grep -c 'ERROR SUMMARY:' \$x_test_out\`
                               summary_ok=\`grep -c 'ERROR SUMMARY: 0 ' \$x_test_out\`
                               # The number of given lines can be zero.
                               # In some cases we can lost valgrind's summary.
                               if test \$summary_all -ne \$summary_ok; then
                                   result=254
                               fi
                               ;;
                esac

                # Write result of the test into the his output file
                echo "Start time   : \$start_time"   >> \$x_test_out
                echo "Stop time    : \$stop_time"    >> \$x_test_out
                echo "Load averages: \$load_avg"     >> \$x_test_out
                echo >> \$x_test_out
                echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" >> \$x_test_out
                echo "@@@ EXIT CODE: \$result" >> \$x_test_out

                if test -f "\$corefile"; then
                    echo "@@@ CORE DUMPED" >> \$x_test_out
                    if test -d "\$bin_dir" -a -f "\$bin_dir/\$x_test"; then
                        mv "\$corefile" "\$bin_dir/\$x_test.core"
                    else
                        rm -f "\$corefile"
                    fi
                fi

                # Write result also on the screen and into the log
                if grep NCBI_UNITTEST_DISABLED \$x_test_out >/dev/null; then
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

                elif echo "\$exec_time" | egrep 'Maximum execution .* is exceeded' >/dev/null || egrep "Maximum execution .* is exceeded" \$x_test_out >/dev/null; then
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

            else  # Run test if it exist
                if \$no_report_err; then
                    echo "ABS --  \$x_cmd"
                    echo "ABS --  \$x_cmd" >> \$res_log
                    count_absent=\`expr \$count_absent + 1\`

                    if test -n "\$NCBI_AUTOMATED_BUILD"; then
                        echo "ABS"         >> "\$x_test_rep"
                        echo "\`date +'$x_date_format'\`" >> "\$x_test_rep"
                        echo "\$x_authors" >> "\$x_test_rep"
                    fi
                fi
            fi

        else  # Check existence of the test's application directory
            if \$no_report_err; then
                # Test application is absent
                echo "ABS -- \$x_work_dir - \$x_test"
                echo "ABS -- \$x_work_dir - \$x_test" >> \$res_log
                count_absent=\`expr \$count_absent + 1\`

                if test -n "\$NCBI_AUTOMATED_BUILD"; then
                    echo "ABS"         >> "\$x_test_rep"
                    echo "\`date +'$x_date_format'\`" >> "\$x_test_rep"
                    echo "\$x_authors" >> "\$x_test_rep"
                fi
            fi
        fi
        
    done  # Run test under all specified check tools   
}

MailToAuthors()
{
   # The limit on the sending email size in Kbytes
   mail_limit=1024
   tmp="./check_mailtoauthors.tmp.\$\$"

   test -z "\$sendmail"  &&  return 0
   test -z "\$1"  &&  return 0
   x_authors=""
   for author in \$1; do
       x_authors="\$x_authors \$author\$domain"
   done
   x_logfile="\$2"
   
   echo '-----------------------'
   echo "Send results of the test \$x_app to \$x_authors"
   echo '-----------------------'
        echo "To: \$x_authors"
        echo "Subject: [WATCHERS] \$x_app | \$signature"
        echo
        echo \$x_cmd
        echo
   echo "cmd = \$sendmail \$x_authors"
   
   COMMON_LimitTextFileSize \$x_logfile \$tmp \$mail_limit
   {
        echo "To: \$x_authors"
        echo "Subject: [WATCHERS] \$x_app | \$signature"
        echo
        echo \$x_cmd
        echo
        cat \$tmp
        echo 
        cat \$top_srcdir/build_info
   } | \$sendmail \$x_authors
   echo '-----------------------'
   rm -f \$tmp > /dev/null
}

EOF

#//////////////////////////////////////////////////////////////////////////


# Read list with tests
x_tests=`cat "$x_list" | sed -e 's/ /%gj_s4%/g'`
x_test_prev=""

# For all tests
for x_row in $x_tests; do
   # Get one row from list
   x_row=`echo "$x_row" | sed -e 's/%gj_s4%/ /g' -e 's/^ *//' -e 's/ ____ /~/g'`

   # Split it to parts
   x_src_dir="$x_root_dir/src/`echo \"$x_row\" | sed -e 's/~.*$//'`"
   x_test=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_app=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_cmd=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_name=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_files=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_timeout=`echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//'  -e 's/^[^~]*~//' -e 's/~.*$//'`
   x_requires=" `echo "$x_row" | sed -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/^[^~]*~//'  -e 's/^[^~]*~//' -e 's/^[^~]*~//' -e 's/~.*$//'` "
   x_authors=`echo "$x_row" | sed -e 's/.*~//'`

   # Check application requirements
   for x_req in $x_requires; do
      test -f "$x_conf_dir/status/$x_req.enabled"  ||  continue 2
   done

   # Application base build directory
   x_work_dir_tail="`echo \"$x_row\" | sed -e 's/~.*$//'`"
   x_work_dir="$x_compile_dir/$x_work_dir_tail"

   # Copy specified files to the build directory

   if test "$x_import_prj" = "no"; then
      if test ! -z "$x_files"; then
         for i in $x_files ; do
            x_copy="$x_src_dir/$i"
            if test -f "$x_copy"  -o  -d "$x_copy"; then
               cp -prf "$x_copy" "$x_work_dir"
               test -d "$x_work_dir/$i" &&  find "$x_work_dir/$i" -name .svn -print | xargs rm -rf
            else
               echo "Warning:  The copied object \"$x_copy\" should be a file or directory!"
               continue 1
            fi
         done
      fi
   fi

   # Generate extension for tests output file
   if test "$x_test" != "$x_test_prev"; then 
      x_cnt=1
      x_test_ext=""
   else
      x_cnt=`expr $x_cnt + 1`
      x_test_ext="$x_cnt"
   fi
   x_test_prev="$x_test"

#//////////////////////////////////////////////////////////////////////////

   # Write test commands for current test into a shell script file
   cat >> $x_out <<EOF
######################################################################
RunTest "$x_work_dir_tail" \\
        "$x_test" \\
        "$x_app" \\
        "$x_cmd" \\
        "$x_name" \\
        "$x_test_ext" \\
        "$x_timeout" \\
        "$x_authors"
EOF

#//////////////////////////////////////////////////////////////////////////

done # for x_row in x_tests


# Write ending code into the script 
cat >> $x_out <<EOF

if \$no_report_err  &&  \$no_db_load; then
   # Write result of the tests execution
   echo
   echo "Succeeded : \$count_ok"
   echo "Timeout   : \$count_timeout"
   echo "Failed    : \$count_err"
   echo "Absent    : \$count_absent"
   echo
   if test \$count_err -eq 0; then
      echo
      echo "******** ALL TESTS COMPLETED SUCCESSFULLY ********"
      echo
   fi
fi

exit \$count_err
EOF

# Set execute mode to script
chmod a+x "$x_out"

exit 0
