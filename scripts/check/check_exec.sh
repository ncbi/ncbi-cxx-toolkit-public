#! /bin/bash

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Execute check command
#
# Usage:
#    check_exec.sh <timeout> <cmd-line>
#
#    timeout - maximal time of execution "cmd-line" in seconds
#
# Note:
#    For protect infinity execution <cmd-line>, this script terminate 
#    check process if it still executing after "timeout" seconds.
#    Script return <cmd-line> exit code or value above 0 in case error
#    to parent shell.
#
###########################################################################


# Get parameters
timeout=$1
shift
script_dir=`dirname $0`
script_dir=`(cd "$script_dir"; pwd)`

# Run command
"$@" &
pid=$!
trap 'kill $pid' 1 2 15

# Execute time-guard
$script_dir/check_exec_guard.sh $timeout $pid &

# Wait ending of execution
wait $pid > /dev/null 2>&1
status=$?

# Special check for core file on Darwin
if [ $status != 0  -a  `uname -s` = "Darwin" -a -d "/cores"]; then
   sleep 0 > /dev/null 2>&1  &
   end_pid=$!
   p=$pid
   while [ $p -lt $end_pid ]; do
      core="/cores/core.$p"
      if [ -O "$core" ]; then
         # Move the core file to current directory with name "core"
         mv $core ./core > /dev/null 2>&1
         # Save only last core file
         # break
      fi
      p=`expr $p + 1`
   done
fi 

# Return test exit code
exit $status
