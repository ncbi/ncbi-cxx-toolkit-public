#! /bin/sh

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Execute check command
#
# Usage:
#    check_exec.sh <cmd-line>
#
# Note:
#    The <cmd-line> can contains any program or scripts.
#
#    The CHECK_TIMEOUT environment variable defines a "timeout" seconds
#    to execute specified command line. By default timeout is 200 sec.
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

# Make timestamp
timestamp_file=`mktemp /tmp/check_exec_timestamp.XXXXXXXXXX`
touch $timestamp_file
trap "rm -f $timestamp_file" 0 1 2 15

# Save current pid (need for RunID)
echo $$ > check_exec.pid

# Use different kill on Unix and Cygwin
case `uname -s` in
   CYGWIN* ) cygwin=true  ; kill='/bin/kill -f' ;;
   *)        cygwin=false ; kill='kill' ;;
esac

exe="$1"
cmd="$exe"
shift
# Quote all empty arguments
for arg in "$@"; do
   if test -z "$arg"; then
      cmd="$cmd ''"
   else 
      cmd="$cmd $arg"
   fi
done

is_script=''
echo $exe | grep '\.sh' > /dev/null 2>&1 
if test $? -eq 0;  then
   is_script=1
   # Increase timeout for running scripts, because usually they have
   # its own guards for running sub-processes with the same timeout
   timeout=`expr $timeout + 10`   
fi

# Run command.
$cmd &
pid=$!
trap "$kill $pid; rm -f $timestamp_file" 1 2 15

# Execute time-guard
#echo MAIN guard -- timeout=$timeout, pid=$pid, script$is_script
$script_dir/check_exec_guard.sh $timeout $pid $is_script &
guard_pid=$!

# Wait ending of execution
wait $pid > /dev/null 2>&1
status=$?

# Special checks for core files on some platforms
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
   if [ -f core -a -n "$NCBI_CHECK_BACK_TRACE" -a -z "$is_script" ]; then
      printf '\n\n\n\n'
      printf '#\n#\n#\n#\n'
      printf '%.s#' {1..60}
      printf '#\n\n'
      echo "--- Backtrace:" $cmd
      eval $NCBI_CHECK_BACK_TRACE $exe core
      echo
   fi
fi
rm $timestamp_file > /dev/null 2>&1

if [ $cygwin = false ]; then
   $kill $guard_pid >/dev/null 2>&1
fi

# Return test exit code
exit $status
