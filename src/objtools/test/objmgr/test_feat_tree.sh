#! /bin/sh
# $Id$
#

base="${1:-/am/ncbiapdata/test_data/feat_tree}"
if test ! -d $base; then
    echo "Error -- test data dir not found: $base"
    exit 1
fi
if test -d "$1"; then
    shift
fi

d="$base/data"
r="$base/res"
t="."

tool="test_feat_tree $@"

ret=0
do_test() {
    src="$d/$1"
    ref="$r/$1"
    dst="$t/$1.res"
    cmd="$CHECK_EXEC $tool -i $src -o $dst"
    echo $cmd
    if time $cmd; then
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

for f in `cd "$d"; ls`; do
    do_test "$f"
done

echo "Done."
exit $ret
