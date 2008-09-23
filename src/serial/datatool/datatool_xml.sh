#! /bin/sh
# $Id$
#

base="${1:-./testdata}"
if test ! -d $base; then
    echo "Error -- test data dir not found: $base"
    echo "Test will be skipped."
    echo " (for autobuild scripts: NCBI_UNITTEST_SKIPPED)"
    exit 0
fi

d="$base/data"
r="$base/res"
tool="datatool"

for i in "idx" "elink" "note"; do
    echo "$tool" -m "$base/$i.dtd" -vx "$d/$i.xml" -px out
    cmd=`echo "$tool" -m "$base/$i.dtd" -vx "$d/$i.xml" -px out`
    time $cmd
    if test "$?" != 0; then
        echo "datatool failed!"
        exit 1
    fi
    diff -w out "$r/$i.xml"
    if test "$?" != 0; then
        echo "wrong result!"
        exit 1
    fi
done

echo "Done!"
