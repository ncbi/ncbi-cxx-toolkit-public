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


# exit code 88 from the test - if unsupported, run under memory sanitizer or Valgrind

case "`uname -s`" in
 SunOS )
   # exit code 158 -- signal 30 (CPU time exceeded)
   test $test_mem -eq 0    -o  $test_mem -eq 255  ||  exit 1
   test $test_cpu -eq 255  -o  $test_cpu -eq 158  ||  exit 1
   ;;
 Linux )
   test $test_mem -eq 88   -o  $test_mem -eq 1    -o  $test_mem -eq 66   -o  $test_mem -eq 134  -o  $test_mem -eq 255  ||  exit 1
   test $test_cpu -eq 88   -o  $test_mem -eq 1    -o  $test_mem -eq 137  -o  $test_cpu -eq 255  ||  exit 1
   ;;
 Darwin | FreeBSD )
   test $test_mem -eq 88   -o  $test_mem -eq 134  -o  $test_mem -eq 255  ||  exit 1
   test $test_cpu -eq 255  -o  $test_cpu -eq 137  -o  $test_cpu -eq 255  ||  exit 1
   ;;
 *CYGWIN* )
   test $test_mem -eq 88   -o  $test_mem -eq 134  ||  exit 1
   test $test_cpu -eq 88   -o  $test_cpu -eq 134  ||  exit 1
   ;;
 * )
   test $test_mem -eq 255  ||  exit 1
   test $test_cpu -eq 255  -o  $test_cpu -eq 158  ||  exit 1
   ;;
esac

exit 0
