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
#    check_run.sh <make_command_line>
#
# Example:
#    check_run.sh make check_add_r
#
###########################################################################


cmd=$*

# Define name for the check script file
script_name="check.sh"
CHECK_RUN_FILE="`pwd`/$script_name"
export CHECK_RUN_FILE


# Prepare file for test script
if test -f "${CHECK_RUN_FILE}" ; then
   rm -f ${CHECK_RUN_FILE}
fi

cat >> ${CHECK_RUN_FILE} <<EOF
#! /bin/sh

res_journal="${CHECK_RUN_FILE}.journal"
res_log="${CHECK_RUN_FILE}.log"
res_concat="${CHECK_RUN_FILE}.out"

PATH=".:\$PATH"

##  Printout USAGE info and exit

Usage() {
  cat <<EOF_usage

USAGE:  ./$script_name {run | clean | concat}

 run     Run the tests. Create output file ("*.test_out") for each tests, 
         plus journal and log files. 
 clean   Remove all files created during the last "run" and this script itself.
 concat  Concatenate all files created during the last "run" into one big 
         file "\$res_log".

ERROR:  \$1
EOF_usage

    exit 1
}


if test \$# -ne 1 ; then
  Usage "Invalid number of arguments."
fi


###  What to do (cmd-line arg)

method="\$1"


### Action

case "\$method" in
  run )
    ;;
  clean )
    x_files=\`cat \$res_journal | sed -e 's/ /%gj_s4%/g'\`
    for x_file in \${x_files} ; do
      x_file=\`echo "\${x_file}" | sed -e 's/%gj_s4%/ /g'\`
    done
    rm -f \$res_journal \$res_log \$res_concat
    rm -f ${CHECK_RUN_FILE} 
    exit 0
    ;;
  concat )
    rm -f "\$res_concat"
    ( 
    cat \$res_log
    x_files=\`cat \$res_journal | sed -e 's/ /%gj_s4%/g'\`
    for x_file in \${x_files} ; do
      x_file=\`echo "\${x_file}" | sed -e 's/%gj_s4%/ /g'\`
      echo 
      echo 
      cat \$x_file
    done
    ) >> \$res_concat
    exit 0
    ;;
  * )
    Usage "Invalid method name."
    ;;
esac


# Run
count_ok=0
count_err=0
count_absent=0

rm -f "\$res_journal"
rm -f "\$res_log"

EOF


# Run make
echo "======================================================================"
${cmd}
result=$?


# Write make result 
echo "----------------------------------------------------------------------"

if test ${result} -ne 0 ; then
   echo "Error in compiling tests"
   exit ${result}
fi

if test `tail -2 ${CHECK_RUN_FILE} | grep -c res_log` -ne 0 ; then
   echo "Cannot run tests: none found"
   exit 255
else
   cat >> ${CHECK_RUN_FILE} <<EOF
echo
echo "Succeded : \${count_ok}"
echo "Failed   : \${count_err}"
echo "Absent   : \${count_absent}"
echo

if test \${count_err} -eq 0 ; then
   echo
   echo "******** ALL TESTS COMPLETED SUCCESSFULLY ********"
   echo
fi

exit \${count_err}
EOF

fi

chmod a+x ${CHECK_RUN_FILE}


echo "The test script was successfully built."
echo


# Run tests after build flag (Y - run, N - not run, other - ask)
run_check=`echo ${RUN_CHECK} | tr '[a-z]' '[A-Z]' | sed -e 's/^\(.\).*/\1/g'`

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
 n | N )  echo "Run \"${CHECK_RUN_FILE} run\" to launch the tests." ; exit 0 ;;
esac


# Launch the tests
${CHECK_RUN_FILE} run


# Exit
exit $?
