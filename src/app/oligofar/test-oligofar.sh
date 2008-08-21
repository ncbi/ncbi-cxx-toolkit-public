#!/bin/sh
errors=''
./oligofar -V
errors="${errors}$?"
./oligofar -T+ -i NM_012345.reads -d NM_012345.fa -o NM_012345.reads.out -w9 -n1 -L2G "$@" 
errors="${errors},$?"
diff NM_012345.reads.ref NM_012345.reads.out 
errors="${errors},$?"
./oligofar -T+ -i NM_012345.pairs -d NM_012345.fa -o NM_012345.pairs.out -w9 -n1 -L2G "$@"
errors="${errors},$?"
diff NM_012345.pairs.ref NM_012345.pairs.out
errors="${errors},$?"
rm NM_012345.reads.out NM_012345.pairs.out
echo "ERRORS=$errors";
test "$errors" == "0,0,0,0,0"
