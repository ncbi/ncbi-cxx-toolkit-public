#! /bin/sh
# $Id$

test_ncbi_system general;
test_general=$?; echo "exit_code(General-test) = $test_general"
test_ncbi_system cpu;
test_cpu=$?;     echo "exit_code(CPU-test) = $test_cpu"
test_ncbi_system mem;
test_mem=$?;     echo "exit_code(MEM-test) = $test_mem"

test $test_general -eq 0  ||  exit 1

case "`uname -s`" in
 Linux | FreeBSD )
   test $test_cpu -eq 137  ||  exit 1
   test $test_mem -eq 0  -o  $test_mem -eq 255  ||  exit 1
   ;;
 Darwin )
   test $test_cpu -eq 255  ||  exit 1
   test $test_mem -eq 0    ||  exit 1
   ;;
 *CYGWIN* )
   test $test_cpu -eq 3  -a  $test_mem -eq 3  ||  exit 1
   ;;
 * )
   test $test_mem -eq 255  ||  exit 1
   # exit code 158 -- signal 30 (CPU time exceeded)
   test $test_cpu -eq 255  -o  $test_cpu -eq 158  ||  exit 1
   ;;
esac

exit 0
