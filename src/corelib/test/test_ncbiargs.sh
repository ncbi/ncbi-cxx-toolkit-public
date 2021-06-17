#! /bin/sh
# $Id$

count_err=0

RunTest()
{
    expected=$1
    shift
    
    echo
    echo ========================================================================
    echo Command line: "$@"
    
    $CHECK_EXEC test_ncbiargs "$@"
    ret=$?
    echo
    if test $expected = 'OK';  then
       if test $ret -ne 0; then
          echo TEST FAILED
          count_err=`expr $count_err + 1`
          return
       fi
    else
        if test $ret -eq 0; then
          echo EXPECTED APP ERROR, TEST FAILED
          count_err=`expr $count_err + 1`
          return
       fi
    fi
    echo TEST PASSED
}


# default tests

RunTest OK  -help
RunTest OK  -k a -f1 -ko true foo - B False t f
RunTest OK  -ko false -k 100 bar
RunTest OK  -k a -f1 -ko true -kd8 123456789020 foo - B False t f
RunTest OK  --key=123 foo - a -notakey
RunTest ERR 1 2 3

  
# command tests

RunTest ERR runtest1  abc-123-def
RunTest OK  runtest1  abc-123-def -k kvalue

RunTest OK  runtest2  123654  -ka alpha2 -kb true  -ki 34   -kd 1.567
RunTest ERR runtest2  s123654 -ka alpha2 -kb true  -ki 34   -kd 1.567
RunTest ERR runtest2  123654  -ka ...    -kb true  -ki 34   -kd 1.567
RunTest ERR runtest2  123654  -ka alpha2 -kb error -ki 34   -kd 1.567
RunTest ERR runtest2  123654  -ka alpha2 -kb true  -ki 34.1 -kd 1.567

RunTest OK  runtest3  -k1 v1 -f2

RunTest OK  runtest6  No
RunTest OK  runtest6  No 1.23

RunTest ERR runtest7  -f1 str yes 1
RunTest ERR runtest7  -f2 str yes 1
RunTest OK  runtest7  -f2 str yes 1 2
RunTest OK  runtest7  -f3 str yes 1
RunTest ERR runtest7  -f3 str yes 1 2

RunTest OK  runtest8  -datasize 1K   -datetime '2015/2/1'
RunTest OK  runtest8  -datasize=1000 -datetime='2015/2/1 00:00:00'

RunTest OK  runtest9  -k=bar -ki=Foo -i=25
RunTest ERR runtest9  -k=Bar -ki=Foo -i=25
RunTest ERR runtest9  -k=aaa -ki=Foo -i=25
RunTest OK  runtest9  -k=bar -ki=foo -i=25
RunTest ERR runtest9  -k=bar -ki=Foo -i=35

RunTest OK  rt10  -dt1 "1/2/2015 4:15:01.1" -dt2 "2015/1/02 4:15:01"  -dt3 "2015-01-2T4:15:1"  -dt4 "2015-01-02 4:15:01"
RunTest OK  rt10  -dt1 "1/2/2015"           -dt2 "2015/1/02"          -dt3 "2015-01-2"         -dt4 "2015-01-02"
RunTest OK  rt10  -dt1 "11/22/2015 4:15:01" -dt2 "2015/11/22 4:15:01" -dt3 "2015-11-22T4:15:1" -dt4 "2015-11-22 4:15:01"
 

# File based tests 4/5

iFile="./test_ncbiargs.tmp.i.$$"
oFile="./test_ncbiargs.tmp.o.$$"
ioFile="./test_ncbiargs.tmp.io.$$"

RunTest ERR runtest4 -if $iFile -of $oFile

date > $iFile
date > $ioFile

RunTest OK runtest4 -if $iFile -of $oFile
test -f $oFile || exit 4

rm $oFile   
RunTest OK runtest5 -if $iFile -iof $ioFile -of $oFile
test -f $oFile || exit 5
rm $iFile $oFile $ioFile


exit $count_err
