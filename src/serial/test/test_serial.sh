#! /bin/sh

for f in webenv.ent webenv.bin test.asn test.asb test.bin; do
    cp @srcdir@/$f ./
done

@bindir@/serialtest > test.out 2> test.err

for f in webenv.ent webenv.bin test.asn test.asb test.bin; do
    cmp $f ${f}o || echo Error in file ${f}o
done

echo Test finished.
