#! /bin/sh
# $Id$

test_ncbi_system cpu;  test_cpu=$?;  echo "exit_code(CPU-test) = $test_cpu"
test_ncbi_system mem;  test_mem=$?;  echo "exit_code(MEM-test) = $test_mem"
test_ncbi_system pid;  test_pid=$?;  echo "exit_code(PID-test) = $test_pid"

os=`uname -s`

if test "$os" = "Linux" ; then
   test $test_cpu -eq 137  ||  exit 1
   test $test_mem -eq 0  -o  $test_mem -eq 255  ||  exit 1

elif test "$os" = "Darwin" ; then
   test $test_cpu -eq 255  ||  exit 1
   test $test_mem -eq 0    ||  exit 1

elif test -n "`echo $os | grep CYGWIN`" ; then
   test $test_cpu -eq 3  -a  $test_mem -eq 3  ||  exit 1

else
   test $test_mem -eq 255  ||  exit 1
   # exit code 158 -- signal 30 (CPU time exceeded)
   test $test_cpu -eq 255  -o  $test_cpu -eq 158  ||  exit 1
fi

test $test_pid -eq 0  ||  exit 1

exit 0
