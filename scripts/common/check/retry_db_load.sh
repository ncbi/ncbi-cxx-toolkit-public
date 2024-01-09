#! /bin/sh
# $Id$
#
# Author:  Vladimir Ivanov
#
# Helper to load test results to a Database and retry if necessary
# to avoid possible lock/timeout issues.
#
# USAGE:
#     retry_db_load.sh <output_file> <command line to run>
#
# Exit codes:
#     0 - success
#     4 - timeout
#     * - error (any other codes except 0 and 4)


#---------------- Arguments ----------------

script="$0"

if [ "$#" -le 2 ]; then
   echo "USAGE:  `basename $script` <output_file> <command line to run>"
   exit 1
fi

out=$1
shift
cmdline=$*

#---------------- Configuration ----------------

# Maximum number of attempts
max_attempts=3

#---------------- Main -------------------------

rm $out > /dev/null 2>&1

attempt=1
status=0

while [ "$attempt" -le "$max_attempts" ]
do
    # Run build
    (eval $cmdline) >> $out 2>&1
    if [ $? -eq 0 ] ; then
        # Success
        exit 0
    fi
    status=1

    tryagain=false
    grep "Server connection timed out" $out > /dev/null 2>&1  &&  tryagain=true

    # Stop on any other error
    $tryagain  ||  exit $status

    # Try again
    attempt=`expr $attempt + 1`
    if [ "$attempt" -le "$max_attempts" ]; then
       echo >> $out
       echo "retrying..." >> $out
    fi
    status=4
done

exit $status
