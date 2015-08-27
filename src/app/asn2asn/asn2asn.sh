#! /bin/sh
# $Id$
#

base="${1:-./testdata}"
if test ! -d $base; then
    echo "Error -- test data dir not found: $base"
    exit 1
fi
if test -d "$1"; then
    shift
fi

d="$base/data"
r="$base/res"

tool="asn2asn $@"

do_test() {
    cmd="$tool -i $d/$1 -o out"
    echo $cmd
    if time $cmd; then
        :
    else
        echo "asn2asn failed!"
        exit 1
    fi
    if diff -w out $r/$2; then
        :
    elif test -f $r/$2.2 && diff -w out $r/$2.2; then
        :
    elif test -f $r/$2.3 && diff -w out $r/$2.3; then
        :
    else
        echo "failed: diff -w out $r/$2"
        test -f $r/$2.2 && echo "failed: diff -w out $r/$2.2"
        test -f $r/$2.3 && echo "failed: diff -w out $r/$2.3"
        echo "wrong result!"
        exit 1
    fi
    rm out
}

for i in "set.bin -b" "set.ent" "set.xml -X"; do
    do_test "$i -e -s" set.bin
    do_test "$i -e" set.ent
    do_test "$i -e -x" set.xml
done

echo "Done!"
