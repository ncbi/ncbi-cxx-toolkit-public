#!/bin/sh
./oligofar -i NM_012345.reads -d NM_012345.fa -o NM_012345.reads.test -w9 -n1 -L2G || exit 1
./oligofar -i NM_012345.pairs -d NM_012345.fa -o NM_012345.pairs.test -w9 -n1 -L2G || exit 2
diff NM_012345.reads.test NM_012345.reads.out > /dev/null || exit 3
diff NM_012345.pairs.test NM_012345.pairs.out > /dev/null || exit 4
rm NM_012345.reads.test NM_012345.pairs.test
exit 0
