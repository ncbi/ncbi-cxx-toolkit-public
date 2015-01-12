#! /bin/sh

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Auxiliary time-guard script for run check command script
#
# Usage:
#    check_exec_guard.sh <timeout> <pid> <no_checks>
#
#    timeout   - maximal time of execution process with "pid" in seconds
#    no_checks - do not run any debug checks, usually this mean that 'pid'
#                represent some (shell-)script.
# 
# Note:
#    If process with "pid" still execute after "timeout" seconds, that 
#    it will be killed and exit code from this script will be 1. 
#    Otherwise exit code 0 will be returned to parent shell.
#
###########################################################################


# Parameters
timeout=$1
pid=$2
no_checks=$3
sleep_time=5

# Use different kill on Unix and Cygwin
case `uname -s` in
   CYGWIN* ) cygwin=true  ; kill='/bin/kill -f' ;;
   *)        cygwin=false ; kill='kill' ;;
esac

# Wait 
while [ $timeout -gt 0 ]; do
   kill -0 $pid > /dev/null 2>&1  ||  exit 0
   if [ $timeout -lt  $sleep_time ]; then
      sleep_time=$timeout
   fi
   timeout="`expr $timeout - $sleep_time`"
   sleep $sleep_time >/dev/null 2>&1
done

# Time out:

# -- get stacktrace of a running process
if [ -z "$no_checks"  -a  -n "$NCBI_CHECK_STACK_TRACE" ]; then
   printf '\n\n\n\n'
   printf '#\n#\n#\n#\n'
   printf '%.s#' {1..60}
   printf '#\n\n'
   echo "--- Timeout. Stacktrace for pid $pid. Pass (1):"
   eval $NCBI_CHECK_STACK_TRACE $pid
   echo
   echo "--- Timeout. Stacktrace for pid $pid. Pass (2):"
   eval $NCBI_CHECK_STACK_TRACE $pid
   echo
fi

# -- kill the process
echo
echo "Maximum execution time of $1 seconds is exceeded"
echo

$kill $pid > /dev/null 2>&1
if [ $cygwin = false ]; then
   sleep $sleep_time >/dev/null 2>&1
   kill -9 $pid      >/dev/null 2>&1
   sleep $sleep_time >/dev/null 2>&1
   kill -ALRM $PPID  >/dev/null 2>&1
fi

exit 1
