#! /bin/sh

if test -n "@NCBI_C_ncbi@"; then
    testprefix=c
else
    testprefix=cpp
fi

for f in webenv.ent webenv.bin ${testprefix}test.asn ${testprefix}test.asb; do
    cp @srcdir@/$f ./
done
mv ${testprefix}test.asn test.asn
mv ${testprefix}test.asb test.asb

@exec_prefix@/bin/serialtest > test.out 2> test.err

for f in webenv.ent webenv.bin test.asn test.asb; do
    i=`tr -d "\n " < $f`
    o=`tr -d "\n " < ${f}o`
    if test $i != $o; then
        echo Error in file ${f}o;
    fi
done

echo Test finished.
