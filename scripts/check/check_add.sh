#! /bin/sh

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Buid list files to checking in the build tree.
#
# Usage: (Run only from Makefile.meta)
#    check_add.sh <project_srcdir> <project_name>
#
# Example:
#    check_add.sh ~/c++/src/html/test test_jsmenu
#
# Note:
#    1) Environment variable CHECK_RUN_LIST must be set;
#    2) We assume that current work dir is <project_name> in build dir.
#
###########################################################################


# Parameters
x_out=$CHECK_RUN_LIST
x_srcdir=$1
x_test=$2
x_exeext=$3

x_delim=" ____ "

# Check to necessity make test for this application
if test ! -f "$x_srcdir/Makefile.$x_test.app";  then
   echo "Warning: File \"$x_srcdir/Makefile.$x_test.app\" not found."
   exit 0
fi

x_tpath=`echo "$x_srcdir/$x_test" | sed 's%^.*/src/%%'`
if grep -c CHECK_CMD $x_srcdir/Makefile.$x_test.app > /dev/null ; then 
   echo "TEST -- $x_tpath"
else 
   echo "SKIP -- $x_tpath"
   exit 0
fi

# Get app name
x_app=`grep '^ *APP' "$x_srcdir/Makefile.$x_test.app"`
x_app=`echo "$x_app" | sed -e 's/^.*=//' -e 's/^ *//'`$x_exeext

# Get cmd-lines to run test
x_run=`grep '^ *CHECK_CMD' "$x_srcdir/Makefile.$x_test.app"`
x_run=`echo "$x_run" | sed -e 's/^[^=]*=//'`

if test -z "$x_run"; then
   # If command line not defined, then just run the test without parameters
   x_run="$x_test$x_exeext"
fi
x_run=`echo "$x_run" | sed -e 's/ /%gj_s4%/g'`

# Specified files to copy to the build directory
x_files=`grep '^ *CHECK_COPY' "$x_srcdir/Makefile.$x_test.app" | sed -e 's/^.*=//' -e 's/^.[ ]*//'`
# Convert source dir to relative path
x_srcdir=`echo "$x_srcdir" | sed -e 's%^.*/src/%%'`

# Write data about current test into a list file
for x_cmd in $x_run; do
    x_cmd=`echo "$x_cmd" | sed -e 's/%gj_s4%/ /g' | sed -e 's/^ *//' | sed -e 's/\"/\\\\"/g'`
    echo "$x_srcdir$x_delim$x_test$x_delim$x_app$x_delim$x_cmd$x_delim$x_files" >> $x_out
done

exit 0
