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
#    check_make_unix.sh <test_list> <signature> <build_dir> <top_srcdir> <target_dir> <check_script>
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

# Load configuration options
x_check_scripts_dir=`dirname "$0"`
x_scripts_dir=`dirname "$x_check_scripts_dir"`
x_scripts_dir=`dirname "$x_scripts_dir"`
. ${x_check_scripts_dir}/check_setup.cfg


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
x_out_name=$x_out
x_buildcfg=$7

if test "$x_buildcfg" != ""; then
  x_buildcfg="/$x_buildcfg"
fi 

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
#   x_compile_dir="$x_build_dir"
   x_compile_dir="$x_target_dir$x_buildcfg"
fi

if test -z "$x_list"; then
   x_list="$x_target_dir/$res_list"
fi

if test -z "$x_out"; then
   x_out="$x_target_dir$x_buildcfg/$res_out"
else
   x_out="$x_target_dir$x_buildcfg/$x_out_name"
fi

x_script_name=`echo "$x_out" | sed -e 's%^.*/%%'`

# Check for a list file
if test ! -f "$x_list"; then
   echo "Check list file \"$x_list\" not found."
   exit 1 
fi

# Features detection
x_features=""
for f in `ls $x_conf_dir/status/*.enabled | sort -df`; do
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
if test ! -d "${x_target_dir}${x_buildcfg}"; then
  mkdir -p "${x_target_dir}${x_buildcfg}"
fi
cat > $x_out <<EOF
#! /bin/sh

buildcfg="${x_buildcfg}"
checkroot="$x_target_dir"
checkdir="\${checkroot}\${buildcfg}"

root_dir="$x_root_dir"
#build_dir="$x_build_dir"
build_dir="\${checkdir}"
conf_dir="$x_conf_dir"
#compile_dir="$x_compile_dir"
compile_dir="\${checkdir}"
bin_dir="$x_bin_dir\${buildcfg}"
script_dir="$x_scripts_dir"
script="\${checkroot}/$x_out_name"
cygwin=$cygwin
signature="$x_signature"
sendmail=''
domain='@ncbi.nlm.nih.gov'

test -d "\${checkdir}" || mkdir -p "\${checkdir}"
#res_journal="\$script.journal"
#res_log="\$script.log"
#res_list="$x_list"
#res_concat="\$script.out"
#res_concat_err="\$script.out_err"
res_journal="\${checkdir}/${x_out_name}.journal"
res_log="\${checkdir}/${x_out_name}.log"
res_list="\${checkroot}/${x_out_name}.list"
res_log="\${checkdir}/${x_out_name}.out"
res_concat_err="\${checkdir}/${x_out_name}.out_err"

# Define both senses to accommodate shells lacking !
is_run=false
no_run=true
is_report_err=false
no_report_err=true


# Include COMMON.SH
. \${script_dir}/common/common.sh


# Printout USAGE info and exit

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

    exit 1
}

if test \$# -ne 1; then
   Usage "Invalid number of arguments."
fi


# Action

method="\$1"

case "\$method" in
#----------------------------------------------------------
   run )
      is_run=true
      no_run=false
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
   * )
      Usage "Invalid method name \$method."
      ;;
esac


#//////////////////////////////////////////////////////////////////////////


trap "touch \${checkdir}/check.failed; exit 1"  1 2 15
rm \${checkdir}/check.failed \${checkdir}/check.success > /dev/null 2>&1 

# Set log_site for tests
NCBI_APPLOG_SITE=testcxx
export NCBI_APPLOG_SITE

# Include configuration file
. \${checkroot}/check.cfg
if test -z "\$NCBI_CHECK_TOOLS"; then
   NCBI_CHECK_TOOLS="regular"
fi
# Check timeout multiplier (increase default check timeout in x times)
if test -z "\$NCBI_CHECK_TIMEOUT_MULT"; then
   NCBI_CHECK_TIMEOUT_MULT=1
fi
export NCBI_CHECK_TIMEOUT_DEFAULT=$NCBI_CHECK_TIMEOUT_DEFAULT

# Path to test data, used by some scripts and applications
if test -z "\$NCBI_TEST_DATA"; then
    if [ \$cygwin = true ]; then
       NCBI_TEST_DATA=//snowman/win-coremake/Scripts/test_data
    elif test -d /Volumes/ncbiapdata/test_data; then
       NCBI_TEST_DATA=/Volumes/ncbiapdata/test_data
    else
       NCBI_TEST_DATA=/am/ncbiapdata/test_data
    fi
    export NCBI_TEST_DATA
fi
# Add synonym for it, see: include/common/test_data_path.h (CXX-9239)
if test -z "\$NCBI_TEST_DATA_PATH"; then
    NCBI_TEST_DATA_PATH=\$NCBI_TEST_DATA
    export NCBI_TEST_DATA_PATH
fi

# Valgrind/Helgrind configurations
VALGRIND_SUP="\${script_dir}/common/check/valgrind.supp"
VALGRIND_CMD="--tool=memcheck --suppressions=\$VALGRIND_SUP"
HELGRIND_CMD="--tool=helgrind --suppressions=\$VALGRIND_SUP"
if (valgrind --ncbi --help) >/dev/null 2>&1; then
    VALGRIND_CMD="--ncbi \$VALGRIND_CMD" # --ncbi must be the first option!
    HELGRIND_CMD="--ncbi \$HELGRIND_CMD" # --ncbi must be the first option!
fi

# Leak- and Thread- Sanitizers (GCC 7.3, -fsanitize= flags)
LSAN_OPTIONS="suppressions=\${script_dir}/common/check/lsan.supp:exitcode=0"
export LSAN_OPTIONS
TSAN_OPTIONS="suppressions=\${script_dir}/common/check/tsan.supp"
export TSAN_OPTIONS

# Disable BOOST tests to catch asynchronous system failures
# (signals on *NIX platforms or structured exceptions on Windows)
BOOST_TEST_CATCH_SYSTEM_ERRORS=no
export BOOST_TEST_CATCH_SYSTEM_ERRORS

BOOST_TEST_COLOR_OUTPUT=no
export BOOST_TEST_COLOR_OUTPUT

# Export some global vars
top_srcdir="\$root_dir"
export top_srcdir
FEATURES="$x_features"
export FEATURES

# Redirect output for C++ diagnostic framework to stderr,
# except if using under 'export_project' tool.
if test -z "\$NCBI_EXPORT_PROJECT"; then
    NCBI_CONFIG__LOG__FILE="-"
    export NCBI_CONFIG__LOG__FILE
fi

# Add additional necessary directories to PATH: current, build, scripts, utility and $HOME/bin (for Ubuntu).
PATH="\${script_dir}/common/impl:\$NCBI/bin/_production/CPPCORE:\$HOME/bin:.:\${build_dir}:\${bin_dir}:\${PATH}"
export PATH

# Export bin and lib pathes
CFG_BIN="\${conf_dir}/bin\${buildcfg}"
CFG_LIB="\${conf_dir}/lib\${buildcfg}"
export CFG_BIN CFG_LIB

# Define time-guard script to run tests from other scripts
check_exec="\${script_dir}/common/check/check_exec.sh"
CHECK_EXEC="\${script_dir}/common/check/check_exec_test.sh"
CHECK_EXEC_STDIN="\$CHECK_EXEC -stdin"
CHECK_SIGNATURE="\$signature"
export CHECK_EXEC
export CHECK_EXEC_STDIN
export CHECK_SIGNATURE

# Debug tools to get stack/back trace (except running under memory checkers)
NCBI_CHECK_STACK_TRACE=''
NCBI_CHECK_BACK_TRACE=''
if test "\$NCBI_CHECK_TOOLS" = "regular"; then
   if (which gdb) >/dev/null 2>&1; then
       NCBI_CHECK_BACK_TRACE='gdb --batch --quiet -ex "thread apply all bt" -ex "quit"'
   fi
   if (which gstack) >/dev/null 2>&1; then
       NCBI_CHECK_STACK_TRACE='gstack'
   fi
   export NCBI_CHECK_BACK_TRACE
   export NCBI_CHECK_STACK_TRACE
fi

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

# Check on linkerd and set backup
echo test | nc -w 1 linkerd 4142 > /dev/null 2>&1
if test \$? -ne 0;  then
   NCBI_CONFIG__ID2SNP__PTIS_NAME="pool.linkerd-proxy.service.bethesda-dev.consul.ncbi.nlm.nih.gov:4142"
   export NCBI_CONFIG__ID2SNP__PTIS_NAME
fi

EOF

if test -n "$x_conf_dir"  -a  -d "$x_conf_dir/lib";  then
   cat >> $x_out <<EOF
# Add a library path for running tests
. \${script_dir}/common/common.sh
COMMON_AddRunpath "\$conf_dir/lib\${buildcfg}"
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


#//////////////////////////////////////////////////////////////////////////

cat >> $x_out <<EOF

# Check for automated build
is_automated=false
is_db_load=false
if test -n "\$NCBI_AUTOMATED_BUILD"; then
   is_automated=true
   if test -n "\$NCBI_CHECK_DB_LOAD"; then
      is_db_load=true
   fi
fi

# Check for some executables
have_ncbi_applog=false
if (ncbi_applog generate) >/dev/null 2>&1; then
   have_ncbi_applog=true
fi
have_uptime=false
if (which uptime) >/dev/null 2>&1; then
   have_uptime=true
fi


#//////////////////////////////////////////////////////////////////////////


# Run
count_ok=0
count_err=0
count_timeout=0
count_absent=0
count_total=0

if \$is_run; then
   rm -f "\$res_journal"
   rm -f "\$res_log"
   #rm -f "$x_build_dir/test_stat_load.log"
fi

# Set app limits:
# Only if $NCBI_CHECK_SETLIMITS not set to 0 before, or not configured with -with-max-debug.
# Some tools that use this configure flag, like AddressSanitizer, can fail if limited.

is_max_debug=false
if test -f "\${conf_dir}/status/MaxDebug.enabled"; then
   is_max_debug=true
fi
if test "\$NCBI_CHECK_SETLIMITS" != "0"  -a  ! \$is_max_debug; then
   ulimit -c 1000000
   ulimit -n 8192
   if [ \$cygwin = false ]; then
       if test "\$NCBI_CHECK_TOOLS" = "regular"; then
          ulimit -v 48000000
       else
          # Increase memory limits if run under check tool
          ulimit -v 64000000
       fi
   fi
fi


# Run one test

RunTest()
{
    # Parameters
    IFS=';'; rargs=(\$1); unset IFS;
    x_work_dir_tail=\${rargs[0]};
    x_test=\${rargs[1]};
    x_app=\${rargs[2]};
    x_run=\${rargs[3]};
    x_alias=\${rargs[10]};
    x_name=\${rargs[4]};
    x_files=\${rargs[5]};
    x_timeout=\${rargs[6]};
    x_requires=\${rargs[7]};
    x_authors=\${rargs[8]};
    x_resources=\${rargs[9]};
    r_id=\$2

    test -z "\$x_timeout"  &&  x_timeout=\$NCBI_CHECK_TIMEOUT_DEFAULT
    x_work_dir="\$checkdir/\$x_work_dir_tail"
    x_done="\$checkdir/~\$x_name"
    x_wlog_dir="\$x_work_dir"
    x_work_dir="\$x_work_dir/~\$x_name"
    x_log="$x_tmp/\$\$.~\$x_name.out"
    x_info="$x_tmp/\$\$.~\$x_name.info"

    if test -f "/etc/nologin"; then
        echo "Nologin detected, probably host going to reboot. Skipping test:" \$x_name
        touch "\$checkdir/~RUN_CHECKS.next"
        return 0
    fi
    if \$is_report_err; then
        # Authors are not defined for this test
        if test -z "\$x_authors"; then
            touch "\$checkdir/~RUN_CHECKS.next"
            return 0
        fi
    fi
    test -d \${x_work_dir} || mkdir -p \${x_work_dir}


    # Check requirements
    
    x_unmet_requires=""
    for x_req in \$x_requires; do
        t_minus=\${x_req::1}
        if test \$t_minus = "-"; then
           t_req=\${x_req:1}
           if test -f "\$conf_dir/status/\${t_req}.enabled" ; then
              x_unmet_requires="\${x_unmet_requires}\${x_req} "
           fi
        else
           if test ! -f "\$conf_dir/status/\${x_req}.enabled" ; then
              x_unmet_requires="\${x_unmet_requires}\${x_req} "
           fi
        fi
    done
    is_not_skipped=true
    if test -n "\$x_unmet_requires"; then
        is_not_skipped=false
    fi

    if test -n "\$x_resources"; then
        for r in \$x_resources ; do
            if test \$r = "SERIAL"; then
                while ! mkdir "\$checkdir/~\$r.lock" 2>/dev/null
                do
	                sleep 1
                done
                r_count=\`ls \${checkdir}/*.in_progress 2>/dev/null | wc -w | sed -e 's/ //g'\`
                while test \$r_count -gt 0; do
                    sleep 1
                    r_count=\`ls \${checkdir}/*.in_progress 2>/dev/null | wc -w | sed -e 's/ //g'\`
                done
            fi
        done
    fi

    touch "\${x_done}.in_progress"
    if test -f "\${x_done}.done"; then 
        rm -f "\${x_done}.done"
    fi
    touch "\$checkdir/~RUN_CHECKS.next"

    if test -n "\$x_resources"; then
        for r in \$x_resources ; do
            if test \$r != "SERIAL"; then
                while ! mkdir "\$checkdir/~\$r.lock" 2>/dev/null
                do
	                sleep 1
                done
            fi
        done
    fi

    if test -n "\$x_files"; then
        for i in \$x_files ; do
            x_copy="\$root_dir/src/\$x_work_dir_tail/\$i"
            if test -f "\$x_copy"  -o  -d "\$x_copy"; then
                cp -rf "\$x_copy" "\$x_work_dir"
                test -d "\$x_work_dir/\$i" &&  find "\$x_work_dir/\$i" -name .svn -print | xargs rm -rf
            else
                echo "[\$x_work_dir_tail] \$x_name: Warning:  The copied object \"\$x_copy\" should be a file or directory!"
                continue
            fi
        done
    fi

    # Run test under all specified check tools   
    for tool in \$NCBI_CHECK_TOOLS; do

        saved_phid=''

        tool_lo=\`echo \$tool | tr '[A-Z]' '[a-z]'\`
        tool_up=\`echo \$tool | tr '[a-z]' '[A-Z]'\`
        
        case "\$tool_lo" in
            regular | valgrind | helgrind ) ;;
                             * ) continue ;;
        esac
        
        x_cmd="[\$r_id/\$x_TestsTotal \$x_work_dir_tail] \$x_name"
        if test \$tool_lo = "regular"; then
           #x_cmd="[\$x_work_dir_tail] \$x_name"
           x_test_out="\$x_wlog_dir/\$x_name.test_out\$x_ext"
           x_test_rep="\$x_wlog_dir/\$x_name.test_rep\$x_ext"
           x_boost_rep="\$x_wlog_dir/\$x_name.boost_rep\$x_ext"
        else
           #x_cmd="[\$x_work_dir_tail] \$tool_up \$x_name"
           x_test_out="\$x_wlog_dir/\$x_name.test_out\$x_ext.\$tool_lo"
           x_test_rep="\$x_wlog_dir/\$x_name.test_rep\$x_ext.\$tool_lo"
           x_boost_rep="\$x_wlog_dir/\$x_name.boost_rep\$x_ext.\$tool_lo"
        fi

   
        if \$is_run && \$is_automated; then
           echo "\$signature \$NCBI_CHECK_OS_NAME" > "\$x_test_rep"
           echo "\$x_work_dir_tail" >> "\$x_test_rep"
           echo "\$x_run" >> "\$x_test_rep"
           echo "\$x_alias" >> "\$x_test_rep"
           NCBI_BOOST_REPORT_FILE="\$x_boost_rep"
           if $cygwin; then
               export NCBI_BOOST_REPORT_FILE="\$(cygpath -w "\$x_boost_rep")"
           else
               export NCBI_BOOST_REPORT_FILE
           fi
        fi

        # Check existence of the test's application directory
        if test -d "\$x_work_dir"; then

            # Goto the test's directory 
            cd "\$x_work_dir"

            # Run test if it exist
            if test -f "\$x_app" -o -f "\$bin_dir/\$x_app"; then

                _RLD_ARGS="-log \$x_log"
                export _RLD_ARGS

                # Fix empty parameters (replace "" to \"\", '' to \'\')
                x_run_fix=\`echo "\$x_run" | sed -e 's/""/\\\\\\\\\\"\\\\\\\\\\"/g' -e "s/''/\\\\\\\\\\'\\\\\\\\\\'/g"\`

                # Define check tool variables
                NCBI_CHECK_TOOL=\`eval echo "\$"NCBI_CHECK_\${tool_up}""\`
                case "\$tool_lo" in
                regular  ) ;;
                valgrind | helgrind ) 
                           if test "\$tool_lo" = "valgrind"; then
                              NCBI_CHECK_TOOL="\$NCBI_CHECK_VALGRIND \$VALGRIND_CMD" 
                           else
                              NCBI_CHECK_TOOL="\$NCBI_CHECK_VALGRIND \$HELGRIND_CMD" 
                           fi
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
                (
                    echo "======================================================================"
                    echo "\$x_name"
                    echo "======================================================================"
                    echo 
                    if test "\$x_run" != "\$x_name"; then
                       echo "Command line: \$x_run"
                       echo 
                    fi
                    if test -n "\$NCBI_CHECK_ENV_SETTINGS"; then
                       echo "NCBI_CHECK_ENV_SETTINGS:"
                       for env in \$NCBI_CHECK_ENV_SETTINGS; do
                           echo "    \$env = \`eval echo '$'\$env\`"
                       done
                       echo
                    fi
                ) > \$x_test_out 2>&1

                # Remove old core file if any
                corefile="\$x_work_dir/core"
                rm -f "\$corefile" > /dev/null 2>&1
                rm -f check_exec.pid > /dev/null 2>&1

                # Generate PHID and SID, to use it by any application in the current test,
                # and for loading test statistics later (test_stat_load -> ncbi_applog),
                # to have same values in Applog.
                logfile=\$NCBI_CONFIG__LOG__FILE
                NCBI_CONFIG__LOG__FILE=
                export NCBI_CONFIG__LOG__FILE
                if \$have_ncbi_applog; then
                   eval "\`ncbi_applog generate -phid -sid -format=shell-export | tr -d '\r'\`"
                   if \$is_run && \$is_db_load; then
                      # Use generated PHID for test statistics, and sub-PHID.1 for test itself
                      saved_phid=\$NCBI_LOG_HIT_ID
                      NCBI_LOG_HIT_ID=\$NCBI_LOG_HIT_ID.1
                      export NCBI_LOG_HIT_ID
                      # Create a file in the cirrent directory with initial sub-PHID
                      # (will be incremented by $CHECK_EXEC, if any)
                      echo "0" > \$NCBI_LOG_HIT_ID
                   fi
                fi
                NCBI_CONFIG__LOG__FILE=\$logfile
                export NCBI_CONFIG__LOG__FILE

                # Run check
                start_time="\`date +'$x_date_format'\`"

                if \$is_not_skipped; then
                        
                # Use separate shell to run test.
                # This will allow to know execution time for applications with timeout.
                # Also, process guard works better if used after "time -p".
                launch_sh="$x_tmp/launch.\$\$.~\$x_name.sh"
cat > \$launch_sh <<EOF_launch
#! /bin/sh
exec time -p \$check_exec \`eval echo \$xx_run\`
EOF_launch
                chmod a+x \$launch_sh
                \$launch_sh >\$x_log 2>&1
                result=\$?
                stop_time="\`date +'$x_date_format'\`"
                if \${have_uptime}; then
                    load_avg="\`uptime | sed -e 's/.*averages*: *\(.*\) *$/\1/' -e 's/[, ][, ]*/ /g'\`"
                else
                    load_avg="unavailable"
                fi
                rm \$launch_sh

                LC_ALL=C sed -e '/ ["][$][@]["].*\$/ {
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
                exec_time=\`\${checkroot}/sysdep.sh tl 7 \$x_log | tr '\n\r' '%%' | tr -d '\000-\037' | tr  -d '\176-\377'\`
                echo \$exec_time | egrep 'real [0-9]|Maximum execution .* is exceeded' > /dev/null 2>&1 
                if test \$? -eq 0;  then
                    exec_time=\`echo \$exec_time |   \\
                                sed -e 's/%%/%/g'    \\
                                    -e 's/%$//'      \\
                                    -e 's/%/, /g'    \\
                                    -e 's/[ ] */ /g' \\
                                    -e 's/^.*\(Maximum execution [0-9][0-9]* is exceeded\).*$/\1/' \\
                                    -e 's/^.*\(real [0-9][0-9]*[.][0-9][0-9]*\)/\1/' \\
                                    -e 's/\(sys [0-9][0-9]*[.][0-9][0-9]*\).*/\1/'\`
                else
                    exec_time='unparsable timing stats'
                fi
               
                rm -f \$x_log

                # Analize check tool output
                case "\$tool_lo" in
                    valgrind | helgrind ) 
                               summary_all=\`grep -c 'ERROR SUMMARY:' \$x_test_out\`
                               summary_ok=\`grep -c 'ERROR SUMMARY: 0 ' \$x_test_out\`
                               # The number of given lines can be zero.
                               # In some cases we can lost valgrind's summary.
                               if test \$summary_all -ne \$summary_ok; then
                                   result=254
                               fi
                               ;;
                    * )
                               # GCC Sanitizer can fails with a 0 exit code
                               if \$is_max_debug; then
                                   grep '==ERROR: AddressSanitizer:' \$x_test_out > /dev/null 2>&1 
                                   if test \$? -eq 0;  then
                                      result=253
                                   fi
                               fi
                    
                esac

                else # skipped
                    echo "Test disabled due unmet check requires: \$x_unmet_requires" >> \$x_test_out
                    echo NCBI_UNITTEST_DISABLED >> \$x_test_out
                    echo >> \$x_test_out
                    # Define necessary variables to write into the test footer
                    result=99
                    stop_time="\$start_time"
                    load_avg=""
                    exec_time="real 0.0, user 0.0, sys 0.0"
                    runid=""
                fi

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

                echo "t_cmd=\\"\$x_cmd\\"" > \$x_info
                echo "t_test_out=\\"\$x_test_out\\"" >> \$x_info
                echo "t_exec_time=\\"\$exec_time\\"" >> \$x_info
                echo "t_result=\\"\$result\\"" >> \$x_info

                # Write results
                if \$is_automated; then
                    if grep NCBI_UNITTEST_DISABLED \$x_test_out >/dev/null; then
                        echo "DIS" >> "\$x_test_rep"
                    elif grep NCBI_UNITTEST_SKIPPED \$x_test_out >/dev/null; then
                        echo "SKP" >> "\$x_test_rep"
                    elif grep NCBI_UNITTEST_TIMEOUTS_BUT_NO_ERRORS \$x_test_out >/dev/null; then
                        echo "TO" >> "\$x_test_rep"
                    elif echo "\$exec_time" | egrep 'Maximum execution .* is exceeded' >/dev/null || egrep "Maximum execution .* is exceeded" \$x_test_out >/dev/null; then
                        echo "TO" >> "\$x_test_rep"
                    elif test \$result -eq 0; then
                        echo "OK" >> "\$x_test_rep"
                    else
                        echo "ERR" >> "\$x_test_rep"
                    fi
                    echo "\$start_time" >> "\$x_test_rep"
                    echo "\$result"     >> "\$x_test_rep"
                    echo "\$exec_time"  >> "\$x_test_rep"
                    echo "\$x_authors"  >> "\$x_test_rep"
                    echo "\$load_avg"   >> "\$x_test_rep"
                    echo "\$runid"      >> "\$x_test_rep"
                fi

            else  # Run test if it exist
                if \$is_run; then
                    echo "t_cmd=\\"\$x_cmd\\"" > \$x_info
                    echo "t_test_out=\\"\$x_test_out\\"" >> \$x_info
                    if \$is_automated; then
                        echo "ABS"         >> "\$x_test_rep"
                        echo "\`date +'$x_date_format'\`" >> "\$x_test_rep"
                        echo "\$x_authors" >> "\$x_test_rep"
                    fi
                fi
            fi

        else  # Check existence of the test's application directory
            if \$is_run; then
                # Test application is absent
                echo "t_cmd=\\"\$x_cmd\\"" > \$x_info
                echo "t_test_out=\\"\$x_test_out\\"" >> \$x_info
                if \$is_automated; then
                    echo "ABS"         >> "\$x_test_rep"
                    echo "\`date +'$x_date_format'\`" >> "\$x_test_rep"
                    echo "\$x_authors" >> "\$x_test_rep"
                fi
            fi
        fi

        # Load test results to Database and Applog immediately after a test.
        # Always load test results for automated builds on a 'run' command.
        
        if \$is_run && \$is_db_load; then
            while ! mkdir "\$checkdir/~test_stat_load.lock" 2>/dev/null
            do
	            sleep 1
            done
           if test -n "\$saved_phid";  then
              NCBI_LOG_HIT_ID=\$saved_phid
              export NCBI_LOG_HIT_ID
           fi
           case \`uname -s\` in
              CYGWIN* )
                test_stat_load "\$(cygpath -w "\$x_test_rep")" "\$(cygpath -w "\$x_test_out")" "\$(cygpath -w "\$x_boost_rep")" "\$(cygpath -w "\$top_srcdir/build_info")" >> "\${checkdir}/test_stat_load.log" 2>&1 ;;
              IRIX* )
                test_stat_load.sh "\$x_test_rep" "\$x_test_out" "\$x_boost_rep" "\$top_srcdir/build_info" >> "\${checkdir}/test_stat_load.log" 2>&1 ;;
              * )
                test_stat_load "\$x_test_rep" "\$x_test_out" "\$x_boost_rep" "\$top_srcdir/build_info" >> "\${checkdir}/test_stat_load.log" 2>&1 ;;
            esac
            echo >> "\${checkdir}/test_stat_load.log" 2>&1
            rm -rf "\$checkdir/~test_stat_load.lock"
        fi
        if test \$is_run  -a  -n "\$saved_phid"; then
            rm -f \$saved_phid* > /dev/null 2>&1
        fi
        
    done  # Run test under all specified check tools   

    if test -n "\$x_resources"; then
        rev_resources=""
        for r in \$x_resources ; do
            rev_resources="\$r \${rev_resources}"
        done
        for r in \$rev_resources ; do
            rm -rf "\$checkdir/~\$r.lock"
        done
    fi
    if test -f \$x_info; then 
        mv \$x_info "\${x_done}.done"
    else
        touch "\${x_done}.done"
    fi
    rm "\${x_done}.in_progress"
    rm -rf "\${x_work_dir}" 2>/dev/null
    if test \$? -ne 0;  then
        sleep 1
        rm -rf "\${x_work_dir}" 2>/dev/null
    fi
}

MailToAuthors()
{
   # The limit on the sending email size in Kbytes
   mail_limit=1024

   test -z "\$sendmail"  &&  return 0
   test -z "\$1"  &&  return 0
   x_authors=""
   for author in \$1; do
       x_authors="\$x_authors \$author\$domain"
   done
   x_logfile="\$2"
   tmp="./check_mailtoauthors.tmp.\$\$.\`basename \$x_logfile\`"
   
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

ProcessDone()
{
    while true; do
        sleep 2
        p_done=\`ls \${checkdir}/*.started 2>/dev/null\`
        if test -n "\$p_done"; then
            for p_file in \$p_done; do
                cat \$p_file
                rm \$p_file
            done
        fi
        p_done=\`ls \${checkdir}/*.done 2>/dev/null\`
        if test -n "\$p_done"; then
            for p_file in \$p_done; do
                source \$p_file
                if test ! -e "\$t_test_out"; then
                    echo "ABS --  \$t_cmd"
                    echo "ABS --  \$t_cmd" >> \$res_log
                    count_absent=\`expr \$count_absent + 1\`
                    rm -f \$p_file
                    continue
                fi
                echo "\$t_test_out" >> \$res_journal
                count_total=\`expr \$count_total + 1\`
                # Write result on the screen
                if grep NCBI_UNITTEST_DISABLED \$t_test_out >/dev/null; then
                    echo "DIS --  \$t_cmd"
                    echo "DIS --  \$t_cmd" >> \$res_log
                    count_absent=\`expr \$count_absent + 1\`

                elif grep NCBI_UNITTEST_SKIPPED \$t_test_out >/dev/null; then
                    echo "SKP --  \$t_cmd"
                    echo "SKP --  \$t_cmd" >> \$res_log
                    count_absent=\`expr \$count_absent + 1\`

                elif grep NCBI_UNITTEST_TIMEOUTS_BUT_NO_ERRORS \$t_test_out >/dev/null; then
                    echo "TO  --  \$t_cmd"
                    echo "TO  --  \$t_cmd" >> \$res_log
                    count_timeout=\`expr \$count_timeout + 1\`

                elif echo "\$t_exec_time" | egrep 'Maximum execution .* is exceeded' >/dev/null || egrep "Maximum execution .* is exceeded" \$t_test_out >/dev/null; then
                    echo "TO  --  \$t_cmd     (\$t_exec_time)"
                    echo "TO  --  \$t_cmd     (\$t_exec_time)" >> \$res_log
                    count_timeout=\`expr \$count_timeout + 1\`

                elif test \$t_result -eq 0; then
                    echo "OK  --  \$t_cmd     (\$t_exec_time)"
                    echo "OK  --  \$t_cmd     (\$t_exec_time)" >> \$res_log
                    count_ok=\`expr \$count_ok + 1\`

                else
                    echo "ERR [\$t_result] --  \$t_cmd     (\$t_exec_time)"
                    echo "ERR [\$t_result] --  \$t_cmd     (\$t_exec_time)" >> \$res_log
                    count_err=\`expr \$count_err + 1\`
                fi
                rm -f \$p_file
            done
        else
            if test -e "\$checkdir/~DONE"; then
                break
            fi
        fi
    done
    rm "\$checkdir/~DONE"
}

AddJob()
{
    a_pid="\$1"
    a_name="\$2"
    a_id="\$3"

    if test "\${a_pid}" -gt 0; then
        echo "        Start \$a_id: \${a_name} (\$a_pid)" > "\$checkdir/~\$a_name.started"
        while test ! -e "\$checkdir/~RUN_CHECKS.next"; do
            if test -e "\$checkdir/~SERIAL.lock"; then
                sleep 2
            else
                sleep .1
            fi
        done
        rm "\$checkdir/~RUN_CHECKS.next"
    fi
    while test -e "\$checkdir/~SERIAL.lock"; do
        sleep 2
    done

    if test "\${a_pid}" -gt 0; then
        if test -n "\$NTEST_PARALLEL_LEVEL"; then
            a_maxjob=\$NTEST_PARALLEL_LEVEL
        elif test -n "\$CTEST_PARALLEL_LEVEL"; then
            a_maxjob=\$CTEST_PARALLEL_LEVEL
        elif test -n "\$NUMBER_OF_PROCESSORS"; then
            a_maxjob=\$NUMBER_OF_PROCESSORS
        else
            a_maxjob=4
        fi
    else
        a_maxjob=0
    fi

    a_run=\`ls \${checkdir}/*.in_progress 2>/dev/null\`
    a_run=\`echo \$a_run | wc -w | sed -e 's/ //g'\`
    if test "\${a_run}" -ne "\${a_run}"; then
echo "error:  1 a_run = \$a_run"
        a_run=0
    fi

    while test "\$a_run" -ge "\$a_maxjob"; do
        sleep 1
        a_run=\`ls \${checkdir}/*.in_progress 2>/dev/null\`
        a_run=\`echo \$a_run | wc -w | sed -e 's/ //g'\`
        if test "\${a_run}" -ne "\${a_run}"; then
echo "error:  2 a_run = \$a_run"
            break
        fi
        if test "\${a_run}" -le 0; then
            break
        fi
    done
    if test "\${a_maxjob}" -le 0; then
        touch "\$checkdir/~DONE"
    fi
}

#//////////////////////////////////////////////////////////////////////////
# Run tests

RunJobs()
{
    res_list=\$1
    x_i=0
    while read x_row; do
        x_row=\`echo "\$x_row" | sed -e 's/ ____ /;/g' | sed -e 's/ ____/;/g' | sed -e 's/ ;/;/g'\`
        IFS=';'; arrIN=(\$x_row); unset IFS;
        x_name=\${arrIN[4]};
        x_i=\`expr \$x_i + 1\`

        RunTest "\$x_row" "\$x_i" &
        AddJob "\$!" "\$x_name" "\$x_i"
    done < "\$res_list"
    AddJob "0" "" "" ""
}

rm -rf "\$checkdir/~*" 2>/dev/null
locks=\`ls -d \${checkdir}/~*.lock 2>/dev/null | wc -w | sed -e 's/ //g'\`
if test \$locks -ne 0; then
  echo "ERROR: there are locks in \${checkdir}" 1>&2
  exit 1
fi
x_test=""
x_TestsTotal=\`cat "\$res_list" | wc -l | sed -e 's/ //g'\`
x_START=\$SECONDS
RunJobs "\$res_list" &
ProcessDone
x_DURATION=\`expr \$SECONDS - \$x_START\`
echo
echo "Total Test time (real) = \${x_DURATION} sec"

if \$is_run; then
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

if test \$count_err -eq 0; then
   touch \${checkdir}/check.success
else 
   touch \${checkdir}/check.failed
fi

exit \$count_err
EOF

# Set execute mode to script
chmod a+x "$x_out"

exit 0
