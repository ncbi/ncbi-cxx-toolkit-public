#! /bin/sh

SYSTEM_NAME=`uname -s`
SYSTEM_MAJOR_RELEASE=`uname -r | sed -e 's/\..*//'`

## Do not run odbc-based tests  on FreeBSD 4.x. 
if test $SYSTEM_NAME = "FreeBSD" -a $SYSTEM_MAJOR_RELEASE = 4 ; then
    exit
fi

run_sybase_app.sh $1 $2 $3 $4 $5

exit $?
