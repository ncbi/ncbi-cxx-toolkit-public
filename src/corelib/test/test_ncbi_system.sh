#! /bin/sh
# $Id$

$CHECK_EXEC test_ncbi_system general
test_general=$?
echo "exit_code(General-test) = $test_general"

$CHECK_EXEC test_ncbi_system mem
test_mem=$?
echo "exit_code(MEM-test) = $test_mem"

$CHECK_EXEC test_ncbi_system cpu
test_cpu=$?
echo "exit_code(CPU-test) = $test_cpu"

test $test_general -eq 0  ||  exit 1

case "`uname -s`" in
 SunOS )
   # exit code 158 -- signal 30 (CPU time exceeded)
   test $test_mem -eq 0    -o  $test_mem -eq 255  ||  exit 1
   test $test_cpu -eq 255  -o  $test_cpu -eq 158  ||  exit 1
   ;;
 Linux | FreeBSD )
   test $test_mem -eq 0    -o  $test_mem -eq 255  ||  exit 1
   test $test_cpu -eq 255  -o  $test_cpu -eq 137  ||  exit 1
   ;;
 Darwin )
   test $test_mem -eq 0    ||  exit 1
   test $test_cpu -eq 255  ||  exit 1
   ;;
 *CYGWIN* )
   test $test_mem -eq 3  -o  $test_mem -eq 127  -o  $test_mem -eq 129  -o  $test_mem -eq 134  -o  $test_cpu -eq 139  ||  exit 1
   test $test_cpu -eq 3  -o  $test_cpu -eq 127  -o  $test_cpu -eq 129  -o  $test_cpu -eq 134  -o  $test_cpu -eq 139  ||  exit 1
   ;;
 * )
   test $test_mem -eq 255  ||  exit 1
   test $test_cpu -eq 255  -o  $test_cpu -eq 158  ||  exit 1
   ;;
esac

exit 0
