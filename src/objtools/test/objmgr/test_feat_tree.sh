#! /bin/sh
# $Id$
#

. ncbi_test_data
base="${1:-$NCBI_TEST_DATA/feat_tree}"
if test ! -d $base; then
    echo "Error -- test data dir not found: $base"
    exit 1
fi
if test -d "$1"; then
    shift
fi

t="."

tool="test_feat_tree $@"

ret=0
do_test() {
    d="$2"
    r="$3"
    src="$d/$1"
    ref="$r/$1"
    dst="$t/$1.res"
    cmd="$tool -i $src -o $dst"
    echo $cmd
    if time $CHECK_EXEC $cmd; then
        :
    else
        echo "failed: $cmd"
        ret=1
        return
    fi
    if diff -w "$dst" "$ref"; then
        :
    else
        echo "wrong result: $cmd"
        ret=1
        return
    fi
    rm "$dst"
}

d="$base/data"
r="$base/res"
x="$base/data3"
for f in `cd "$d"; ls`; do
    if test -f "$x/$f"; then
	echo "Skipping test $d/$f replaced with $x/$f"
	continue
    fi
    do_test "$f" "$d" "$r"
done

d="$base/data2"
r="$base/res2"
for f in `cd "$d"; ls`; do
    do_test "$f" "$d" "$r"
done

d="$base/data3"
r="$base/res3"
for f in `cd "$d"; ls`; do
    do_test "$f" "$d" "$r"
done

echo "Done."
exit $ret
