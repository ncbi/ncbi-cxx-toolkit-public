#! /bin/sh

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Compile a script and copy necessary files to run test in the build tree.
#
# Usage: (Run only from Makefile.meta)
#    check_add.sh <project_srcdir> <project_name>
#
# Example:
#    check_add.sh ~/c++/src/html/test test_jsmenu
#
# Note:
#    1) Environment variable CHECK_RUN_FILE must be set;
#    2) We assume that current work dir is <project_name> in build dir.
#
###########################################################################


# Parameters
x_out=$CHECK_RUN_FILE
x_srcdir=$1
x_test=$2
x_exeext=$3


# Check to test application
if test ! -f ${x_srcdir}/Makefile.${x_test}.app ;  then
   echo "Warning: File \"${x_srcdir}/Makefile.${x_test}.app\" not found."
   exit 0
fi

x_tpath=`echo "${x_srcdir}/${x_test}" | sed 's%^.*/src/%%'`
if grep -c CHECK_CMD $x_srcdir/Makefile.$x_test.app > /dev/null ; then 
   echo "TEST -- ${x_tpath}"
else 
   echo "SKIP -- ${x_tpath}"
   exit 0
fi


# Copy specified files to the build (current) directory
x_files=`grep '^ *CHECK_COPY' ${x_srcdir}/Makefile.${x_test}.app`
x_files=`echo "$x_files" | sed -e 's/^.*=//'`

if test ! -z "${x_files}" ; then
   for i in ${x_files} ; do
      x_copy=${x_srcdir}/${i}
      if test -f ${x_copy}  -o  -d ${x_copy} ; then
         cp -prf "${x_copy}" .
      else
         echo "Warning:  \"${x_copy}\" must be file or directory!"
         exit 0
      fi
   done
fi


# Get app name
x_app=`grep '^ *APP' ${x_srcdir}/Makefile.${x_test}.app`
x_app=`echo "$x_app" | sed -e 's/^.*=//'`${x_exeext}

# Get cmd-lines to run test
x_run=`grep '^ *CHECK_CMD' ${x_srcdir}/Makefile.${x_test}.app`
x_run=`echo "$x_run" | sed -e 's/^[^=]*=//'`

if test -z "${x_run}"; then
    # If command line not defined, then just run the test without parameters
#    x_run=`echo "${x_app}" | sed -e 's/^ *//g'`
    x_run="${x_test}${x_exeext}"
fi
x_run=`echo "${x_run}" | sed -e 's/ /%gj_s4%/g'`


# Write test commands for current test into a shell script file
x_pwd=`pwd`
x_cnt=1
test_out="$x_pwd/$x_test.test_out"

for x_cmd in ${x_run} ; do
   x_cmd=`echo "${x_cmd}" | sed -e 's/%gj_s4%/ /g' | sed -e 's/^ *//' | sed -e 's/\"/\\\\"/g'`

cat >> $x_out <<EOF
#######################################################

if test -d "$x_pwd"; then

   echo "$test_out" >> \${res_journal}
   (
     echo "======================================================================"
     echo "$x_cmd"
     echo "======================================================================"
     echo 
   ) > $test_out 2>&1
   rm -f $x_pwd/core

   cd $x_pwd
   x_cmd=\`echo "$x_pwd/$x_cmd" | sed 's%^.*/build/%%'\`

   if test -f $x_app; then
      ($x_cmd) >> $test_out 2>&1
      result=\$?

      echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" >> $test_out
      echo "@@@ EXIT CODE: \${result}" >> $test_out
      if test -f $x_pwd/core ; then
         echo "@@@ CORE DUMPED" >> $test_out
         rm -f $x_pwd/core
      fi

      x_cmd=\`echo "$x_pwd/$x_cmd" | sed 's%^.*/build/%%'\`
      if test \${result} -eq 0 ; then
         echo "OK  --  \$x_cmd"
         echo "OK  --  \$x_cmd" >> \${res_log}
         count_ok=\`expr \${count_ok} + 1\`
      else
         echo "ERR --  \$x_cmd"
         echo "ERR [\${result}] --  \$x_cmd" >> \${res_log}
         count_err=\`expr \${count_err} + 1\`
      fi
   else
      echo "ABS --  \$x_cmd"
      echo "ABS --  \$x_cmd" >> \${res_log}
      count_absent=\`expr \${count_absent} + 1\`
   fi
else
   echo "ABS -- $x_pwd/ - $x_test"
   count_absent=\`expr \${count_absent} + 1\`
fi

EOF
   x_cnt=`expr ${x_cnt} + 1`
   test_out="$x_pwd/$x_test.test_out$x_cnt"
done

exit 0
