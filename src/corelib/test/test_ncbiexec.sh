#! /bin/sh
# $Id$

os=`uname -s`

test_ncbiexec;  exit_code=$?;  echo "exit code = $exit_code"

if test -n "`echo $os | grep CYGWIN`" ; then
   test $exit_code -eq 0  -o  $exit_code -eq 99  ||  exit 1
else
   test $exit_code -eq 99  ||  exit 1
fi

exit 0
