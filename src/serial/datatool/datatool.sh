#! /bin/sh
# $Id$
#

base="${TEST_DATA_DIR:-/net/sampson/a/coremake/test_data/objects}"
d="$base/data"
r="$base/res"

tool="./datatool"

asn="$base/all.asn"

do_test() {
    eval args=\""$1"\"
    shift
    file="$1"
    shift
    echo "$tool" -m "$asn" $args out "$@"
    time "$tool" -m "$asn" $args out "$@"
    if test "$?" != 0; then
        echo "datatool failed!"
        exit 1
    fi
    cmp out "$r/$file"
    if test "$?" != 0; then
        echo "wrong result!"
        exit 1
    fi
    rm out
}

for i in "-t Seq-entry -d $d/set.bin" "-v $d/set.ent" "-vx $d/set.xml"; do
    do_test "$i -e" set.bin "$@"
    do_test "$i -p" set.ent "$@"
    do_test "$i -px" set.xml "$@"
done
for i in "-t Bioseq-set -d $d/phg.bin" "-v $d/phg.ent" "-vx $d/phg.xml"; do
    do_test "$i -e" phg.bin "$@"
    do_test "$i -p" phg.ent "$@"
    do_test "$i -px" phg.xml "$@"
done
echo "Done!"
