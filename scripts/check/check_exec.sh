#! /bin/sh

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
cmd=$*

# Run command
($cmd) &
pid=$!
trap 'kill $pid' 1 2 15
sleep 1

# Execution time-guard
check_exec_guard=${NCBI:-/netopt/ncbi_tools}/c++/scripts/check_exec_guard.sh
if ! test -x $check_exec_guard ; then
  check_exec_guard="check_exec_guard.sh"
fi
$check_exec_guard $timeout $pid &
trap 'kill $! $pid' 1 2 15

# Wait ending of execution
wait $pid

# Return exit code
exit $?
