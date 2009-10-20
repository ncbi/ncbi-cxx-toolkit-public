#! /bin/sh

pcre=test_pcre
out="/tmp/$pcre.$$"
errcode=0

trap 'rm -f $out' 0 1 2 15


# Run PCRE tests

echo --------------- Test 1 ----------------------
$pcre -q testdata/testinput1 > $out
diff testdata/testoutput1 $out  ||  errcode=1

case "`uname -s`" in
 *CYGWIN* )
   # Windows specific test3
   echo --------------- Test 3 ----------------------
   $pcre -q testdata/wintestinput3 > $out
   diff testdata/wintestoutput3 $out  ||  errcode=3
   ;;

  FreeBSD )
   # Do not run test2 on FreeBSD. 
   # Some parts of test 2 can exceed available memory in some configurations.
   ;;

 * )
   echo --------------- Test 2 ----------------------
   $pcre -q testdata/testinput2 > $out
   diff testdata/testoutput2 $out  ||  errcode=2
   
#   echo --------------- Test 3 ----------------------
#   Original test3 is disabled on all platforms: there is no fr_FR locale.
#   $pcre -q testdata/testinput3 > $out
#   diff testdata/testoutput3 $out  ||  errcode=3
   ;;
esac

echo --------------- Test 4 ----------------------
$pcre -q testdata/testinput4 > $out
diff testdata/testoutput4 $out  ||  errcode=4

echo --------------- Test 5 ----------------------
$pcre -q testdata/testinput5 > $out
diff testdata/testoutput5 $out  ||  errcode=5

echo --------------- Test 6 ----------------------
$pcre -q testdata/testinput6 > $out
diff testdata/testoutput6 $out  ||  errcode=6

echo --------------- Test 7 ----------------------
$pcre -q -dfa testdata/testinput7 > $out
diff testdata/testoutput7 $out  ||  errcode=7

echo --------------- Test 8 ----------------------
$pcre -q -dfa testdata/testinput8 > $out
diff testdata/testoutput8 $out  ||  errcode=8

echo --------------- Test 9 ----------------------
$pcre -q -dfa testdata/testinput9 > $out
diff testdata/testoutput9 $out  ||  errcode=9

echo --------------- Test 10 ----------------------
$pcre -q testdata/testinput10 > $out
diff testdata/testoutput10 $out  ||  errcode=10

rm -f $out

echo exit code = $errcode
exit $errcode
