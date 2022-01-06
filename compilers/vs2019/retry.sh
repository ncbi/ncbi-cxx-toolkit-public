#! /bin/sh
# $Id$
#
# Author:  Vladimir Ivanov
#
# Retry to build C++ Toolkit to workaround "access denied" problems.
#
# USAGE:
#     retry.sh <command line to run>
#
# Command line should have logfile argument specified last.
# It will be used to check output from Visual Studio.


#---------------- Arguments ----------------

script="$0"


if [ "$#" -le 2 ]; then
   echo "USAGE:  `basename $script` <command line to run>"
   exit 1
fi

# Command line to run build
cmdline=$*

# Logfile is always a last argument in the command line,
# we will use it for a check on errors.
for last; do true; done
logfile=$last


#---------------- Configuration ----------------

# Maximum number of attempts to build toolkit
max_attempts=3


#---------------- Main -------------------------

attempt=1
status=0
s=''


while [ "$attempt" -le "$max_attempts" ]
do
    eval $cmdline >/dev/null
    status=$?
    if [ $status -eq 0 ] ; then
        # Success
        exit 0
    fi

    # Check on a logfile presence
    if [ ! -f $logfile ] ; then
        echo "$0: Logfile $logfile not found.";
        exit 1

    fi 

    # Check on a system errors (TRACKER, access denied)
    grep "TRACKER : error TRK0002:.*Access is denied" $logfile > /dev/null 2>&1
    if [ $? -ne 0 ] ; then
        # Some other error
        exit $status
    fi

    # Try again
    echo "$0: Retrying after $attempt failed attempt$s.";
    attempt=`expr $attempt + 1`
    s='s'

done

echo
echo "$0: Exceeded limit of $max_attempts tries.";
echo

exit $status
