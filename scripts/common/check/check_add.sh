#! /bin/sh
# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Buid list files to checking in the build tree.
#
# Usage: (Run only from Makefile.meta)
#    check_add.sh <project_srcdir> <project_name> <signature> <rel_srcdir>
#
# Example:
#    check_add.sh ~/c++/src/html/test test_jsmenu  \
#                 GCC_295-Debug--sparc-sun-solaris2.8-serpens html/test
#
# Note:
#    1) Environment variable CHECK_RUN_LIST must be set;
#    2) We assume that current work dir is <project_name> in build dir.
#    3) If environment variable CHECK_USE_IGNORE_LIST is 'Y' that the name 
#       of the current test will be checked in ignore list
#       (src/check/ignore.lst). And if it present in list that this test 
#       will be skipped. By default this variable is 'Y'.
#
###########################################################################

# Parameters
x_out=$CHECK_RUN_LIST
x_srcdir=`(cd "$1"; pwd)`
x_test=$2
x_signature=$3
x_srcdir_rel=$4
x_use_ignore_list=${CHECK_USE_IGNORE_LIST-Y}
x_delim=" ____ "
# Default timeout for check (in seconds)
x_timeout_default=200


# Attempt to determine the relative source directory if necessary.
# (This logic assumes that the first occurrence of src as a path
# component will be the relevant one.)
case "$x_srcdir_rel" in
    /* | '' ) 
        x_srcdir_rel=`echo "$x_srcdir" | perl -pe 's|^.*?/src/||'`
        ;;
esac

# Check to necessity make test for this application
if test ! -f "$x_srcdir/Makefile.$x_test.app";  then
   echo "Warning: File \"$x_srcdir/Makefile.$x_test.app\" not found."
   exit 0
fi

# Get app name
x_app=`grep '^ *APP[ =]' "$x_srcdir/Makefile.$x_test.app"`
x_app=`echo "$x_app" | sed -e 's/^.*=//' -e 's/^ *//' -e 's/\s*$//'`

x_tpath=$x_srcdir_rel/$x_test
if grep -c '^ *CHECK_CMD' $x_srcdir/Makefile.$x_test.app > /dev/null ; then 
   # Check ignore list
   x_use_ignore_list=`echo $x_use_ignore_list | tr '[a-z]' '[A-Z]' | sed -e 's/^\(.\).*/\1/g'`
   if test "$x_use_ignore_list"='Y' ; then
      root_dir=`echo "$x_srcdir" | sed 's%/src/.*$%%'`
      ignore_list="$root_dir/src/check/ignore.lst"
      if [ -f "$ignore_list" ]; then
         x_signature=`echo $x_signature | sed 's/-[^-]*$//'`
         # Find "test signature"
         s=`grep " *$x_srcdir_rel/$x_app *$x_signature" $ignore_list`
         if [ -n "$s" ]; then
            # Found. Check on possible date:
            dc=`date '+%Y%m%d'`
            dt=`echo $s | sed 's|\(....\)-\(..\)-\(..\):.*|\1\2\3|'`
            if [ -z "$dt" -o  "$dc" -lt "$dt" ]; then
               # Date is defined, but not arrived yet
               echo "SKIP -- $x_tpath"
               exit 0
            fi
         fi
      fi
   fi
   echo "TEST -- $x_tpath"
else 
   echo "SKIP -- $x_tpath"
   exit 0
fi

# Get cmd-lines to run test
x_run=`grep '^ *CHECK_CMD' "$x_srcdir/Makefile.$x_test.app"`
if test -z "$x_run"; then
   # If command line not defined, then just run the test without parameters
   x_run="$x_app"
fi
x_run=`echo "$x_run" | sed -e 's/ /%gj_s4%/g'`

# Specified files to copy to the build directory
x_files=`grep '^ *CHECK_COPY' "$x_srcdir/Makefile.$x_test.app" | sed -e 's/^.*=//' -e 's/^[ ]*//'`
# Get application's check timeout
x_timeout=`grep '^ *CHECK_TIMEOUT' "$x_srcdir/Makefile.$x_test.app" | sed -e 's/^.*=//' -e 's/^[ ]*//'`
x_timeout="${x_timeout:-$x_timeout_default}"
# Get application's requirement
reg_build=`grep '^ *REQUIRES' "$x_srcdir/Makefile.$x_test.app" | sed -e 's/^.*=//' -e 's/^[ ]*//'`
req_check=`grep '^ *CHECK_REQUIRES' "$x_srcdir/Makefile.$x_test.app" | sed -e 's/^.*=//' -e 's/^[ ]*//'`
x_requires="$reg_build $req_check"

# Get list of watchers
x_makefile="$x_srcdir/Makefile.$x_test.app"
x_watchers=`grep '^ *WATCHERS' "$x_makefile" | sed -e 's/^.*=//' -e 's/,/ /g' -e 's/;/ /g' -e 's/^[ ]*//'`
if [ -z "$x_watchers" ]; then
   # Watchers not found in the project's makefile, continue searching in Makefile.in files
   f=$x_makefile;
   while [ -n "$f" ]; do
      f=`dirname $f`
      test $f = '.' &&  f=''
      x_makefile="$f/Makefile.in"
      if [ -f "$x_makefile" ] ; then
         x_watchers=`grep '^ *WATCHERS' "$x_makefile" | sed -e 's/^.*=//' -e 's/,/ /g' -e 's/;/ /g' -e 's/^[ ]*//'`
         test -n "$x_watchers"  &&  f=''
      else
         f=''
      fi
   done
fi

# Write data about current test into the list file
for x_cmd in $x_run; do
    x_cmd=`echo "$x_cmd"  | sed -e 's/%gj_s4%/ /g'`
    x_name=`echo "$x_cmd" | sed -e 's/^.*CHECK_NAME *= *\([A-Za-z0-9_]*\).*/\1/' -e 's/.*CHECK_CMD.*//'`
    x_cmd=`echo "$x_cmd"  | sed -e 's/ *\/CHECK_NAME.*//' -e 's/^[^=]*=//' -e 's/^ *//'  -e 's/\"/\\\\"/g'`
    echo "$x_srcdir_rel$x_delim$x_test$x_delim$x_app$x_delim$x_cmd$x_delim$x_name$x_delim$x_files$x_delim$x_timeout$x_delim$x_requires$x_delim$x_watchers" >> $x_out
done

exit 0
