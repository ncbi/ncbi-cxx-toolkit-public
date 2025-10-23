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
    echo `date "+%m/%d/%y %H:%M:%S"` '   Command line: ' $cmdline
    echo 
    # Remove logfile between attemps, VS append new output if the file exists
    rm $logfile > /dev/null 2>&1

    # Run build
    eval $cmdline
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

    tryagain=false
    grep "TRACKER : error TRK0002:.*Access is denied" $logfile > /dev/null 2>&1  &&  tryagain=true

    if [ !$tryagain ] ; then
        grep "fatal error C1090: PDB API call failed" $logfile > /dev/null 2>&1  &&  tryagain=true
    fi
    if [ !$tryagain ] ; then
        grep "LINK : fatal error LNK1104: cannot open file" $logfile > /dev/null 2>&1  &&  tryagain=true
    fi
    # Stop on any other error
    $tryagain  ||  exit $status

    # Try again
    echo "$0: Retrying after $attempt failed attempt$s.";
    attempt=`expr $attempt + 1`
    s='s'

done

echo
echo "$0: Exceeded limit of $max_attempts tries.";
echo "$0: Build status = $status";
echo

exit $status
