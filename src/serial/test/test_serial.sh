@script_shell@

#
# $Id$
# Author:  Eugene Vasilchenko
# 


if test -n "@NCBI_C_ncbi@" ; then
    testprefix=c
else
    testprefix=cpp
fi

for f in webenv.ent webenv.bin ${testprefix}test_serial.asn ${testprefix}test_serial.asb ; do
    cp @srcdir@/$f ./
done

mv ${testprefix}test_serial.asn test_serial.asn
mv ${testprefix}test_serial.asb test_serial.asb

@build_root@/bin/test_serial > test_serial.out 2> test_serial.err

for f in webenv.ent webenv.bin test_serial.asn test_serial.asb ; do
    tr -d "\n " < ${f}  > ${f}.$$.tmp
    tr -d "\n " < ${f}o > ${f}o.$$.tmp
    cmp -s ${f}.$$.tmp ${f}o.$$.tmp
    if test $? -ne 0 ; then
        echo "Error in file ${f}o"
    fi
    rm ${f}.$$.tmp ${f}o.$$.tmp
done

echo "Test finished."
