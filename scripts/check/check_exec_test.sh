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
#    cmd-line - command line to execute.
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

tmp=./$2.stdin.$$

# Reinforce timeout
ulimit -t `expr $timeout + 5` > /dev/null 2>&1

# Run command; enforce a minimum effective run time to avoid races,
# since not all shells support waiting for children that have already
# terminated even though POSIX requires them to.
if [ "X$1" = "X-stdin" ]; then
  trap 'rm -f $tmp' 1 2 15
  cat - > $tmp
  shift
  (sleep 1 ;  exec "$@" < $tmp) &
else
  (sleep 1 ;  exec "$@") &
fi
pid=$!
trap 'kill $pid; rm -f $tmp' 1 2 15

# Execute time-guard
$script_dir/check_exec_guard.sh $timeout $pid &

# Wait ending of execution
wait $pid > /dev/null 2>&1
status=$?
rm -f $tmp > /dev/null 2>&1

# Return test exit code
exit $status
