#! /bin/bash

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Auxiliary time-guard script for run check command script
#
# Usage:
#    check_exec_guard.sh <timeout> <pid>
#
#    timeout - maximal time of execution process with "pid" in seconds
# 
# Note:
#    If process with "pid" still execute after "timeout" seconds, that 
#    it will be killed and exit code from this script will be 255. 
#    Otherwise exit code 0 will be returned to parent shell.
#
###########################################################################


# Parameters
timeout=$1
pid=$2

# Recalculate timeout 
sleep_time=5
timeout="`expr $timeout / $sleep_time`"

# Wait ending of process with "pid" or the specified time maximum
time=0
while [ $time -lt $timeout ]; do
  kill -0 $pid > /dev/null 2>&1  ||  exit 0
  time="`expr $time + 1`"
  sleep $sleep_time
done

# Time out, kill the process
kill $pid > /dev/null 2>&1

exit 0
