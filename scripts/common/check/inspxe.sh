#!/bin/bash

# =========================================================
# Wrapper to run test under Intel Inspector XE.
# Command line:
#      inspxe <test_name> <args>
# =========================================================

script="$0"
script_dir=`dirname $script`
script_dir=`(cd "$script_dir"; pwd)`
suppress_dir="$(cygpath -w $script_dir)\\inspxe-suppressions"


# Check Intel Inspector presence

if test -z "$INSPECTOR_2019_DIR"; then
   echo "Cannot find Intel Inspector 2019."
#   exit 1 
fi
inspxe=`echo $(cygpath -u $INSPECTOR_2019_DIR) | sed 's| |\ |g'`/bin32/inspxe-cl


# Result directory name

rd="${1}.i"

n=1
while [ -d "$rd" ]; do
  n="`expr $n + 1`"
  rd="${1}.i_$n"
done


# Add .exe extension to the first argument, name of the program
exe=$1.exe
shift

# Run test
"$inspxe" -collect mi3 -knob detect-leaks-on-exit=false -knob enable-memory-growth-detection=false -knob enable-on-demand-leak-detection=false -knob still-allocated-memory=false -knob detect-resource-leaks=false -knob stack-depth=16 -result-dir $rd -return-app-exitcode -suppression-file "$suppress_dir" -- $exe "$@"
app_result=$?
sleep 5
if test ! -d $rd; then
   echo
   echo "Intel Inspector fails, mark test as timeouted :" 
   echo "Maximum execution time is exceeded"
   exit $app_result
fi
"$inspxe" -report problems -report-all -result-dir $rd
insp_result=$?


# Exit with application's exit code if check has finished succesfuly

if test $insp_result -eq 0;  then
   exit $app_result
fi
exit $insp_result
