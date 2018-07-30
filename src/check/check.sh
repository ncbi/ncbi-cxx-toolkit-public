#! /bin/sh

# $Id$
# Author:  Denis Vakatov, NCBI 
#
###########################################################################
#
#  Auxilary script -- to be called by "./Makefile.check"
#
#  Command line:
#     check.sh <signature> [reporting args]
#
#  Reporting mode and addresses' args:
#     file:[full:]/abs_fname
#     file:[full:]rel_fname
#     mail:[full:]addr1,addr2,...
#     post:[full:]url
#     stat:addr1,addr2,...
#     watch:addr1,addr2,...
#     debug
#     sendonly
#
#  Checks on $NCBI_CHECK_SPEED_LEVEL environment variable during test stage.
#  It defines maximum number of directories to run checks simultaneously.
#  0 means no MT check, just a regualar check.
#
###########################################################################


#### CONFIG

# Allow to run checks in parallel
use_mt_checks=true
# Maximum number of directories to run checks simultaneously
mt_max_dirs=3
# Sleep timeout between checks on finished tasks (dirs)
mt_sleeptime=30

if [ -n "$NCBI_CHECK_SPEED_LEVEL" ]; then
    if [ $NCBI_CHECK_SPEED_LEVEL -le 1 ]; then
        use_mt_checks=false
    else
        mt_max_dirs=$NCBI_CHECK_SPEED_LEVEL
    fi
fi

# The limit on the sending email size in Kbytes
mail_limit=199


####  ERROR STREAM

err_log="/tmp/check.sh.err.$$"
#exec 2>$err_log
trap "rm -f $err_log" 1 2 15


####  INCLUDE COMMON.SH

script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`
. ${script_dir}/../../scripts/common/common.sh


####  MISC

script_args="$*"

summary_res="check.sh.log"
error_res="check.sh.out_err"
build_info="build_info"

if test -x /usr/sbin/sendmail; then
    sendmail="/usr/sbin/sendmail -oi"
else
    sendmail="/usr/lib/sendmail -oi"
fi


####  FUNCTIONS


# MIME headers intended to keep Outlook Exchange from mangling the body.
# Perhaps we should hardcode UTF-8 rather than ISO 8859-1, but it's a
# moot point since we'll generally just encounter ASCII.

DoMime()
{
    cat <<EOF
MIME-Version: 1.0
Content-Type: text/plain; charset="iso-8859-1"
Content-Transfer-Encoding: binary
EOF
}


# Error reporting

Error()
{
    cat <<EOF 1>&2

-----------------------------------------------------------------------

$script_name $script_args

ERROR:  $1
EOF

    cat $err_log

    kill $$
    exit 1
}


 
####  SPECIAL ARGUMENTS (DEBUG, SENDONLY)

debug="no"
need_check="yes"

for arg in "$@" ; do
    case "$arg" in
      debug )
          set -xv
          debug="yes"
          run_script="/bin/sh -xv"
          ;;
      sendonly )
          need_check="no"
          if test ! -f $summary_res -o ! -f $error_res ; then
              Error "Missing check result files"
          fi
          ;;
    esac
done


####  ARGS

signature="$1"
shift

# If "$need_check" = "no" that files with check results already
# prepared and standing in the current directory. 
# Only post results in this case.

if test "$need_check" = "yes" ; then
    # Default check
    test $# -ge 3  ||  Error "Wrong number of args:  $#"
    make="$1"
    builddir="$2"
    action="$3"
    # build_info can be located in the current directory or 2 levels upper,
    # depends on platform and arguments
    test ! -f $build_info  &&  build_info="$builddir/../../build_info"

    shift
    shift
    shift

    # Use passed $builddir for build_info only, and detect real directory
    # to run tests in case of fake root.
    builddir=`(cd ..; pwd)`
    
    test -d "$builddir"  &&  cd "$builddir"  ||  \
      Error "Cannot change directory to:  $builddir"

    # Check if we have not a library-only build
    
    appornull=`grep 'APP_OR_NULL' $builddir/Makefile.mk`
    if [ -n "$appornull" ]; then
        # No executables - no checks
        grep 'APP_OR_NULL = null' $builddir/Makefile.mk >/dev/null  &&  exit 0
    fi
   
    
    # Process action

    case "$action" in
    all )
        # continue
        ;;
    clean | purge )
        test -x ./check.sh  &&  $run_script ./check.sh clean
        exit 0
        ;;
    * )
        Error "Invalid action:  $action"
        ;;
    esac

    ####  RUN CHECKS

    export MAKEFLAGS
    MAKEFLAGS=
   
    scope='r'
    if grep '^PROJECTS_ =.*[^ ]' Makefile.meta >/dev/null ; then
        scope='p'
    fi
    
    # --- Multi thread tests
    if $use_mt_checks; then
   
        # Prepare global check list
       
        CHECK_RUN_LIST="$builddir/$script_name.list"
        export CHECK_RUN_LIST
        rm $CHECK_RUN_LIST >/dev/null
        "$make" check_add_${scope} RUN_CHECK=N  ||  Error "MAKE CHECK_ADD_${scope} failed"
        test -f $CHECK_RUN_LIST  ||  Error "Error preparing check list"

        # Get list of directories to check
       
        list=`cat "$CHECK_RUN_LIST" | tr -d ' '`
        dirs=''
        for row in $list; do
            dir=`echo "$row" | sed -e 's|/.*$||'`
            dirs="$dirs $dir"
        done 
        dirs=`echo $dirs | tr ' ' '\n' | uniq`

        # Generate check.sh for parallel checks execution. 
        # It should look as any other check.sh, and can be
        # used later to concat results.
        
cat > check.sh <<EOF
#! /bin/sh

builddir="$builddir"
script="\$builddir/check.sh"

res_journal="\$script.journal"
res_log="\$script.log"
res_list="\$script.list"
res_concat="\$script.out"
res_concat_err="\$script.out_err"

mt_max_dirs=$mt_max_dirs
mt_sleeptime=$mt_sleeptime

dirs="$dirs"


#//////////////////////////////////////////////////////////////////////////


# Printout USAGE info and exit

Usage() {
   cat <<EOF_usage

USAGE:  check.sh {run | clean | concat | concat_err}

 run         Run the tests. Create output file ("*.test_out") for each test, 
             plus journal and log files. 
 clean       Remove all files created during the last "run" and this script 
             itself.
 concat      Concatenate all files created during the last "run" into one big 
             file "\$res_log".
 concat_err  Like previous. But into the file "\$res_concat_err" 
             will be added outputs of failed tests only.
 report_err  Report failed tests directly to developers (NCBI/NIH only).

ERROR:  \$1

EOF_usage
    exit 1
}

if test \$# -ne 1; then
   Usage "Invalid number of arguments."
fi


###  What to do (cmd-line arg)

method="\$1"

case "\$method" in
#----------------------------------------------------------
   run )
      # See below 
      ;;
#----------------------------------------------------------
   clean )
      for dir in \$dirs; do
          \$builddir/\$dir/check.sh clean
      done
      rm -f \$res_journal \$res_log \$res_list \$res_concat \$res_concat_err > /dev/null
      rm -f \$script > /dev/null
      exit 0
      ;;
#----------------------------------------------------------
   concat )
      rm -f "\$res_concat"
      ( 
         for dir in \$dirs; do
             cat \$builddir/\$dir/check.sh.log
         done
         for dir in \$dirs; do
             files=\`cat \$builddir/\$dir/check.sh.journal | sed -e 's/ /%gj_s4%/g'\`
             for f in \$files; do
                 f=\`echo "\$f" | sed -e 's/%gj_s4%/ /g'\`
                 echo 
                 echo 
                 cat \$f
             done
         done
      ) >> \$res_concat
      exit 0
      ;;
#----------------------------------------------------------
   concat_err )
      rm -f "\$res_concat_err"
      ( 
         for dir in \$dirs; do
             cat \$builddir/\$dir/check.sh.log | egrep 'ERR \[|TO  -'
         done
         for dir in \$dirs; do
             files=\`cat \$builddir/\$dir/check.sh.journal | sed -e 's/ /%gj_s4%/g'\`
             for f in \$files; do
                 f=\`echo "\$f" | sed -e 's/%gj_s4%/ /g'\`
                 code=\`cat \$f | grep -c '@@@ EXIT CODE:'\`
                 test \$code -ne 0 || continue
                 code=\`cat \$f | grep -c '@@@ EXIT CODE: 0'\`
                 if [ \$code -ne 1 ]; then
                    echo 
                    echo 
                    cat \$f
                 fi
             done
         done
      ) >> \$res_concat_err
      exit 0
      ;;
#----------------------------------------------------------
   report_err )
      # This method works inside NCBI only 
      test "\$NCBI_CHECK_MAILTO_AUTHORS." = 'Y.'  ||  exit 0;
      for dir in \$dirs; do
          \$builddir/\$dir/check.sh report_err
      done
      return 0
      ;;
#----------------------------------------------------------
   * )
      Usage "Invalid method name."
      ;;
esac


#//////////////////////////////////////////////////////////////////////////


Error()
{
    echo "ERROR:  \$1"
    echo
    kill \$\$
    exit 1
}

tasks_pids=''

Cleanup()
{
    touch \$builddir/check.failed
    for p in \$tasks_pids; do 
        kill -9 \$p 2>/dev/null
    done
#    killall -9 -q -e check.sh 2>/dev/null
    exit 1
}



#//////////////////////////////////////////////////////////////////////////

# --- Run

trap "Cleanup"  1 2 15
rm \$builddir/check.failed \$builddir/check.success > /dev/null 2>&1 
rm \$res_journal \$res_log \$res_concat \$res_concat_err > /dev/null 2>&1 

# Task queue and number of tasks in it
tasks=''
tasks_count=0

timestamp="date +'%Y-%m-%d %H:%M'"


# Run checks in directories in parallel (no more than \$mt_max_dirs simultaneously)

for dir in \$dirs; do
    # Run checks in \$dir
    rm \$builddir/\$dir/check.success \$builddir/\$dir/check.failed >/dev/null 2>&1
    echo [\`eval \$timestamp\`] Checking \'\$dir\'
    cd \$builddir/\$dir ||  Error "Cannot change directory to:  \$dir"
    ./check.sh run >/dev/null 2>&1 &
    tasks_pids="\$tasks_pids \$!"

    # Add it into the task list   
    tasks="\$tasks \$dir "
    tasks_count=\`expr \$tasks_count + 1\`
    if [ \$tasks_count -lt \$mt_max_dirs ]; then
        # If have space in queue, run another task, otherwise wait (below) for empty slot
        continue
    fi

    # Wait any running task to finish
    while true; do 
       # Checks on finished tasks
       finished_task=''
       for t in \$tasks; do
           if [ -f "\$builddir/\$t/check.success" -o \\
                -f "\$builddir/\$t/check.failed" ]; then
               finished_task=\$t
               break
           fi
       done
       if [ -n "\$finished_task" ]; then
           echo [\`eval \$timestamp\`] Checking \'\$finished_task\' -- finished
           # Remove task from queue
           tasks=\`echo "\$tasks" | sed -e "s/ \$finished_task //"\`
           tasks_count=\`expr \$tasks_count - 1\`
           # Ready to process next task (dir)
           break;
       else
           # All slots busy -- waiting before check again
           sleep \$mt_sleeptime
       fi
   done
done


# Wait unfinished tasks
wait
tasks_pids=''


# Collect check results and logs

failed=false

for dir in \$dirs; do
    if [ -f "\$builddir/\$dir/check.failed" ]; then
        failed=true
    fi
    cat \$builddir/\$dir/check.sh.journal >> \$res_journal 
    cat \$builddir/\$dir/check.sh.log     >> \$res_log
done

echo
cat \$res_log
echo

if \$failed; then
   touch \$builddir/check.failed
   exit 1
fi

touch \$builddir/check.success
exit 0

EOF

        # Set execute mode to script
        chmod a+x check.sh

        $ Generate check script for every directory in the list
        for dir in $dirs; do
            cd $builddir/$dir ||  Error "Cannot change directory to:  $dir"
            "$make" check_${scope} RUN_CHECK=N  ||  Error "$dir: MAKE CHECK_${scope} failed"
        done
      

    # --- Single thread tests
    else
        "$make" check_${scope} RUN_CHECK=N  ||  Error "MAKE CHECK_${scope} failed"
    fi

    # Run checks
    cd "$builddir"
    $run_script ./check.sh run
fi



####  POST RESULTS

# Parse the destination location list
for dest in "$@" ; do
    type=`echo "$dest" | sed 's%\([^:][^:]*\):.*%\1%'`
    loc=`echo "$dest" | sed 's%.*:\([^:][^:]*\)$%\1%'`

    full=""
    echo "$dest" | grep ':full:' >/dev/null  &&  full="yes"

    case "$type" in
      file )
         echo "$loc" | grep '^/' >/dev/null ||  loc="$builddir/$loc"
         loc=`echo "$loc" | sed 's%{}%'${signature}'%g'`
         if test -n "$full" ; then
             file_list_full="$file_list_full $loc"
         else
             file_list="$file_list $loc"
         fi
         ;;
      mail )
         if test -n "$full" ; then
             mail_list_full="$mail_list_full $loc"
         else
             mail_list="$mail_list $loc"
         fi
         ;;
      stat )
         stat_list="$stat_list $loc"
         ;;
      watch )
         loc=`echo "$loc" | sed 's/,/ /g'`
         watch_list="$watch_list $loc"
         ;;
      debug | sendonly )
         ;;
      * )
         err_list="$err_list BAD_TYPE:\"$dest\""
         ;;
    esac
done


# Compose "full" results archive, if necessary
if test "$need_check" = "yes" ; then
    $run_script ./check.sh concat_err
fi

# Post results to the specified locations
if test -n "$file_list_full" ; then
    for loc in $file_list_full ; do
        cp -p $error_res $loc  ||  err_list="$err_list COPY_ERR:\"$loc\""
    done
fi

# Report check results to authors (Unix only)
if test "$need_check" = "yes" ; then
    if test "$NCBI_CHECK_MAILTO_AUTHORS." = 'Y.' ; then
        $run_script ./check.sh report_err
    fi
fi

if test -n "$file_list" ; then
    for loc in $file_list ; do
        cp -p $summary_res $loc  ||  err_list="$err_list COPY_ERR:\"$loc\""
    done
fi

n_ok=`grep '^OK  --  '                   "$summary_res" | wc -l | sed 's/ //g'`
n_err=`grep '^ERR \[[0-9][0-9]*\] --  '  "$summary_res" | wc -l | sed 's/ //g'`
n_abs=`grep '^ABS --  '                  "$summary_res" | wc -l | sed 's/ //g'`

subject="${signature}  OK:$n_ok ERR:$n_err ABS:$n_abs"
tmp_src="/tmp/check.sh.$$.src"
tmp_dst="/tmp/check.sh.$$.dst"

if test -n "$mail_list_full" ; then
    {
        cat $summary_res
        echo ; echo '%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%' ; echo
        cat $error_res
    } > $tmp_src 
    COMMON_LimitTextFileSize $tmp_src $tmp_dst $mail_limit
    for loc in $mail_list_full ; do
        mailto=`echo "$loc" | sed 's/,/ /g'`
        {
            echo "To: $mailto"
            echo "Subject: [C++ CHECK]  $subject"
            DoMime
            echo
            echo "$subject"
            echo
            test -f $build_info  &&  cat $build_info
            echo
            cat $tmp_dst
        } | $sendmail $mailto  ||  err_list="$err_list MAIL_ERR:\"$loc\""
    done
fi

if test -n "$mail_list"  -a  -s "$error_res" ; then
    COMMON_LimitTextFileSize $error_res $tmp_dst $mail_limit
    for loc in $mail_list ; do
        mailto=`echo "$loc" | sed 's/,/ /g'`
        {
            echo "To: $mailto"
            echo "Subject: [C++ ERRORS]  $subject"
            DoMime
            echo
            echo "$subject"
            echo
            test -f $build_info  &&  cat $build_info
            echo
            cat $tmp_dst
        } | $sendmail $mailto  ||  err_list="$err_list MAIL_ERR:\"$loc\""
    done
fi

# Post check statistics
if test -n "$stat_list" ; then
    COMMON_LimitTextFileSize $summary_res $tmp_dst $mail_limit
    for loc in $stat_list ; do
        mailto=`echo "$loc" | sed 's/,/ /g'`
        {
            echo "To: $mailto"
            echo "Subject: [`date '+%Y-%m-%d %H:%M'`]  $subject"
            DoMime
            echo
            cat $tmp_dst
        } | $sendmail $mailto  ||  err_list="$err_list STAT_ERR:\"$loc\""
    done
fi

# Post errors to watchers
if test -n "$watch_list" ; then
    if test -n "$err_list"  -o  "$debug" = "yes" ; then
        {
            echo "To: $watch_list"
            echo "Subject: [C++ WATCH]  $signature"
            DoMime
            echo
            echo "$err_list"
            echo
            test -f $build_info  &&  cat $build_info
            echo
            echo "========================================"
            COMMON_LimitTextFileSize $err_log $tmp_dst $mail_limit
            cat $tmp_dst
        } | $sendmail $watch_list
    fi
fi

# Cleanup
rm -f $tmp_src $tmp_dst $err_log >/dev/null 2>&1

exit 0
