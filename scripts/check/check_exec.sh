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
sleep 1  &&  "$@"  &
pid=$!

# Execution time-guard
( sleep $timeout  &&  kill $pid ) >/dev/null 2>&1 &
guard_pid=$!
trap 'kill $guard_pid $pid' 1 2 15

# Wait ending of execution
wait $pid
exit=$?

# Terminate the time-guard, test terminated himself
kill $guard_pid >/dev/null 2>&1

# Return test exit code
exit $exit
