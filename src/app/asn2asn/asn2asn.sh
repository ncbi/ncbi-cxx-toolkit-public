#! /bin/sh
# $Id$
#

mainDir=/net/hawthorn/export/home/vasilche/c++/src/objects/asn2asn

d=$mainDir/data
r=$mainDir/res

tool="./asn2asn $@"

do_test() {
    cmd="$tool -i $d/$1 -o out"
    echo $cmd
    if time $cmd; then
        :
    else
        echo "asn2asn failed!"
        exit 1
    fi
    if cmp out $r/$2; then
        :
    else
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
for i in "phg.bin -b" "phg.ent" "phg.xml -X"; do
    do_test "$i -s" phg.bin.sorted
    do_test "$i" phg.ent.sorted
    do_test "$i -x" phg.xml.sorted
done
echo "Done!"
