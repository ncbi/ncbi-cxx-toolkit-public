#! /bin/sh
# $Id$

unamestr=`uname -s`
if [[ "$unamestr" != CYGWIN* ]]; then
  set TZ=America/New_York
  export TZ
fi
$CHECK_EXEC test_ncbitime
