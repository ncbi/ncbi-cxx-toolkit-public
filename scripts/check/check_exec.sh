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
timestamp_file="/tmp/check_exec_timestamp.$$"
touch timestamp_file

# Reinforce timeout
ulimit -t `expr $timeout + 5` > /dev/null 2>&1

# Run command; enforce a minimum effective run time to avoid races,
# since not all shells support waiting for children that have already
# terminated even though POSIX requires them to.
(sleep 1  &  "$@" ;  status=$? ;  wait ;  exit $status) &
pid=$!
trap 'kill $pid' 1 2 15

# Execute time-guard
$script_dir/check_exec_guard.sh $timeout $pid &

# Wait ending of execution
wait $pid > /dev/null 2>&1
status=$?

# Special check for core file on Darwin
if [ $status != 0  -a  `uname -s` = "Darwin" -a -d "/cores" ]; then
   core_files=`find /cores/core.* -newer timestamp_file 2>/dev/null`
   for core in $core_files ; do
      if [ -O "$core" ]; then
         # Move the core file to current directory with name "core"
         mv $core ./core > /dev/null 2>&1
         # Save only last core file
         # break
      fi
   done
fi
rm timestamp_file > /dev/null 2>&1

# Return test exit code
exit $status
