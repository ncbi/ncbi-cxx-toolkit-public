#! /bin/sh
#$Id$
err=0
for acc in AAAA AAAA01 AAAA02 NZ_AAAA NZ_AAAA01 ALWZ AINT AAAA01000001 AAAA01999999; do
    wgs_test -file "$acc" || err="$?"
done
exit "$err"
