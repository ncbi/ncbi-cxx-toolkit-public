#! /bin/sh
# $Id$

unamestr=`uname -s`
if [ "$unamestr" == CYGWIN* ]; then
   # Disable CTime API tests on Windows outside NCBI.
   # Tests depends on a EST timezone, setting TZ environment variable 
   # (as we do for Unix below) doesn't works on Windows.
   if echo "$FEATURES" | grep -v -E 'in-house-resources' > /dev/null; then
       echo "This test is disabled to run outside NCBI."
       echo "NCBI_UNITTEST_DISABLED"
       exit 0
   fi
else
   TZ=America/New_York
   export TZ
fi

$CHECK_EXEC $@
