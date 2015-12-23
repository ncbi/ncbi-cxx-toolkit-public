#! /bin/sh

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Time-guard script to run test program from other scripts (shell).
# 
#
# Usage:
#    check_exec_test.sh [-stdin] <cmd-line>
#
#    -stdin   - option used when application from <cmd-line> attempt
#               to read from std input (cgi applications for example).
#    cmd-line - command line to execute (shell scripts not allowed).
#
# Note:
#    The <cmd-line> should be presented with one application with
#    optional parameters only. Using scripts is not allowed here.
#
#    The CHECK_TIMEOUT environment variable defines a "timeout" seconds
#    to execute specified command line.By default timeout is 200 sec.
#
#    For protect infinity execution <cmd-line>, this script terminate 
#    check process if it still executing after "timeout" seconds.
#    Script return <cmd-line> exit code or value above 0 in case error
#    to parent shell.
#
###########################################################################


# Get parameters
timeout="${CHECK_TIMEOUT:-200}"
script_dir=`dirname $0`
script_dir=`(cd "$script_dir"; pwd)`

is_stdin=false
tmp=./$2.stdin.$$

# Make timestamp
timestamp_file=`mktemp /tmp/check_exec_timestamp.XXXXXXXXXX`
touch $timestamp_file
trap "rm -f $tmp $timestamp_file" 0 1 2 15

# Reinforce timeout
# Note, we cannot set it to $timeout for MT-test, because
# CPU time count for each thread and sum.
ulimit -t `expr $timeout \* 3` > /dev/null 2>&1

# Use different kill on Unix and Cygwin
case `uname -s` in
   CYGWIN* ) cygwin=true  ; kill='/bin/kill -f' ;;
   *)        cygwin=false ; kill='kill' ;;
esac

# Run command.
if [ "X$1" = "X-stdin" ]; then
   is_stdin=true
   cat - > $tmp
   shift
fi

exe="$1"
echo $exe | grep '\.sh' > /dev/null 2>&1 
if test $? -eq 0;  then
   echo "Error: Using CHECK_EXEC macro to run shell scripts is prohibited!"
   exit 1
fi

if $is_stdin; then
   $NCBI_CHECK_TOOL "$@" < $tmp &
else
   $NCBI_CHECK_TOOL "$@" &
fi
pid=$!
trap "$kill $pid; rm -f $tmp $timestamp_file" 1 2 15

# Execute time-guard
#echo TEST guard -- $timeout $pid 
$script_dir/check_exec_guard.sh $timeout $pid &

# Wait ending of execution
wait $pid > /dev/null 2>&1
status=$?
rm -f $tmp > /dev/null 2>&1

# Special checks for core files on some platforms
# Keep this part in sync with code in 'check_exec.sh'.
if [ $status -gt 128  -a  ! -f core ]; then
   if [ `uname -s` = "Darwin" -a -d "/cores" ]; then
     core_files=`find /cores/core.* -newer $timestamp_file 2>/dev/null`
   elif [ `uname -s` = "Linux" ]; then
     if [ -f core.$pid ]; then
       core_files=core.$pid
     else
       core_files=`find core.* -newer $timestamp_file 2>/dev/null`
     fi
   fi
   for core in $core_files ; do
      if [ -O "$core" ]; then
         # Move the core file to current directory with name "core"
         mv $core ./core > /dev/null 2>&1
         # Save only last core file
         # break
      fi
   done
   # Get backstrace from coredump
   if [ -f core -a -n "$NCBI_CHECK_BACK_TRACE" ]; then
      echo "--- Backtrace:" "$@"
      eval $NCBI_CHECK_BACK_TRACE $exe core
      echo
   fi
fi
rm $timestamp_file > /dev/null 2>&1

# Return test exit code
exit $status
