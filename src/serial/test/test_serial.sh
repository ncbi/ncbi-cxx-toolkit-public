#! /bin/sh
#
# $Id$
# Author:  Eugene Vasilchenko
# 

testprefix=c
echo " $FEATURES " | grep " C-Toolkit " > /dev/null  ||  testprefix=cpp

for f in webenv.ent webenv.bin ${testprefix}test_serial.asn ${testprefix}test_serial.asb ; do
    cp $top_srcdir/src/serial/test/$f ./
done

mv ${testprefix}test_serial.asn test_serial.asn
mv ${testprefix}test_serial.asb test_serial.asb

$CHECK_EXEC test_serial > test_serial.out 2> test_serial.err  ||  exit 1

status=0

for f in webenv.ent webenv.bin test_serial.asn test_serial.asb ; do
    tr -d "\r\n " < ${f}  > ${f}.$$.tmp
    tr -d "\r\n " < ${f}o > ${f}o.$$.tmp
    cmp -s ${f}.$$.tmp ${f}o.$$.tmp
    if test $? -ne 0 ; then
        echo "Error in file ${f}o"
        status=1
    fi
    rm ${f}.$$.tmp ${f}o.$$.tmp
done

test $status -eq 0  ||  exit 2
echo "Test finished."
exit 0
