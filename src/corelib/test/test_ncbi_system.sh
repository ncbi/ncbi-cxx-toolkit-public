#! /bin/sh
# $Id$


./test_ncbi_system cpu;  test_cpu=$?;  echo "exit_code(CPU-test) = $test_cpu"
./test_ncbi_system mem;  test_mem=$?;  echo "exit_code(MEM-test) = $test_mem"

if test "`uname -s`" = "Linux" ; then
   test $test_cpu -eq 137  -a  $test_mem -eq 255  ||  exit 1
else
   test $test_cpu -eq 255  -a  $test_mem -eq 255  ||  exit 1
fi

exit 0
