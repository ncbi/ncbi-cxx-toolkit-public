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

# Reinforce timeout
ulimit -t `expr $timeout + 5` > /dev/null 2>&1

# Run command
if [ "X$1" = "X-stdin" ]; then
  cat - > $0.stdin.$$
  shift
  $@ < $0.stdin.$$ &
  sleep 1 # Reduce the chance of removing the file before it's read....
  rm $0.stdin.$$ > /dev/null 2>&1
else
  "$@" &
fi
pid=$!
trap 'kill $pid; rm $0.stdin.$$ > /dev/null 2>&1' 1 2 15

# Execute time-guard
$script_dir/check_exec_guard.sh $timeout $pid &

# Wait ending of execution
wait $pid > /dev/null 2>&1
status=$?

# Return test exit code
exit $status
