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
#    For protect infinity execution <cmd-line>, it run together with 
#    time-quard script, which terminate check process if it still executing
#    after "timeout" seconds. 
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
( $@ ) &
pid=$!

# Execution time-guard
check_exec_guard="$script_dir/check_exec_guard.sh"
$check_exec_guard $timeout $pid &
trap 'kill $! $pid' 1 2 15

# Wait ending of execution
wait $pid

# Return exit code
exit $?
