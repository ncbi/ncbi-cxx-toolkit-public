#! /bin/sh

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Build script file for run tests in the build tree. Using "make".
# Scripts exit code is equival to count of tests, executed with errors.
#
# Usage: (Run only from Makefile.meta)
#    check_run.sh <signature> <build_dir> <abs_top_srcdir> <make_cmd_line>
#
###########################################################################

signature=$1
build_dir=$2
top_srcdir=$3
shift
shift
shift
cmd=$*
script_dir=`dirname $0`
script_dir=`(cd "$script_dir"; pwd)`
make_check_script="$script_dir/check_make_unix.sh"

# Define name for the check script file
script_name="check.sh"
CHECK_RUN_FILE="`pwd`/$script_name"
export CHECK_RUN_FILE
CHECK_RUN_LIST="`pwd`/$script_name.list"
export CHECK_RUN_LIST

# Delete all test file list
rm -f "$CHECK_RUN_FILE" > /dev/null
rm -f "$CHECK_RUN_LIST" > /dev/null

# Run make
echo "======================================================================"
$cmd
result=$?
echo "----------------------------------------------------------------------"

# Check tests list build result 
if test $result -ne 0 ; then
   echo "Error in collecting tests."
   exit $result
fi

# Check script build result
if test ! -f "$CHECK_RUN_LIST"; then
   echo "Cannot run tests: none found."
   exit 255
fi

# Build script on base of check-list
echo "Building check script..."
$make_check_script "$CHECK_RUN_LIST" "$signature" "$build_dir" "$top_srcdir" "" "$CHECK_RUN_FILE"

# Check script build result
if test $? -ne 0 -o \
   `$build_dir/sysdep.sh tl 2 $CHECK_RUN_FILE | grep -c res_log` -ne 0 ; then
   echo "Error in compiling check script."
   exit 255
fi

echo "Done."
echo

# Run tests after build flag (Y - run, N - don't run, other - ask)
run_check=`echo $RUN_CHECK | tr '[a-z]' '[A-Z]' | sed -e 's/^\(.\).*/\1/g'`

case "$run_check" in
  Y )
    answer='Y' ;;
  N )
    answer='N' ;;
  * )
    echo "Do you want to run the tests right now? [y/n]"
    read answer
    echo ;;
esac

case "$answer" in
 n | N )  echo "Run \"$CHECK_RUN_FILE run\" to launch the tests." ; exit 0 ;;
esac


# Launch the tests
$CHECK_RUN_FILE run


# Exit
exit $?
