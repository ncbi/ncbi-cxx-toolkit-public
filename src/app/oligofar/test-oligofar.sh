#!/bin/sh
./oligofar -T+ -i NM_012345.reads -d NM_012345.fa -o NM_012345.reads.test -w9 -n1 -L2G "$@" || exit 11
./oligofar -T+ -i NM_012345.pairs -d NM_012345.fa -o NM_012345.pairs.test -w9 -n1 -L2G "$@" || exit 12
diff NM_012345.reads.test NM_012345.reads.out > /dev/null || exit 21
diff NM_012345.pairs.test NM_012345.pairs.out > /dev/null || exit 22
rm NM_012345.reads.test NM_012345.pairs.test
exit 0
