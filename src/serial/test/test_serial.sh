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

for f in webenv.ent webenv.bin ${testprefix}test.asn ${testprefix}test.asb ; do
    cp @srcdir@/$f ./
done

mv ${testprefix}test.asn test.asn
mv ${testprefix}test.asb test.asb

@build_root@/bin/serialtest > test.out 2> test.err

for f in webenv.ent webenv.bin test.asn test.asb ; do
    tr -d "\n " < ${f}  > ${f}.$$.tmp
    tr -d "\n " < ${f}o > ${f}o.$$.tmp
    cmp -s ${f}.$$.tmp ${f}o.$$.tmp
    if test $? -ne 0 ; then
        echo "Error in file ${f}o"
    fi
    rm ${f}.$$.tmp ${f}o.$$.tmp
done

echo "Test finished."
