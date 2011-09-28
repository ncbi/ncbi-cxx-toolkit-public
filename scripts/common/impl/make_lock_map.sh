#!/bin/sh
# $Id$

PATH=/bin:/usr/bin
export PATH

act=false
cache_dir='.#SRC-cache'
lock_map='.#lock-map'
files=Makefile.*.[la][ip][bp]
test=test
builddir=
rel_srcdir=

# These arguments enable complementing non-trivial lock maps with .hints
# files (in .../build/build-system) defining order-only prerequisites for use
# by Makefile.flat, which can then avoid intra-build contention altogether.
while [ $# != 0 ]; do
    case "$1" in
        -bd ) builddir=$2   ; shift 2 ;;
        -sd ) rel_srcdir=$2 ; shift 2 ;;
    esac
done

common=_`basename $PWD`_common

if [ -d "$cache_dir" ]; then
    cd $cache_dir
    for x in *; do
        if test \! -f ../$x; then
            rm -f $x
            act=true
        fi
    done
    cd ..
else
    mkdir "$cache_dir"
fi

(test Makefile.in -ef Makefile.in) 2>/dev/null  ||  test=/usr/bin/test
if nawk 'BEGIN { exit 0 }' 2>/dev/null; then
    awk=nawk
else
    awk=awk
fi

for x in $files; do
    if [ "x$x" = "x$files" ]; then
        # echo "No application or library makefiles found in $PWD."
        exit 0
    elif $test \! -f "$cache_dir/$x" -o "$cache_dir/$x" -ot "$x"; then
        $awk -F= '{ sub("#.*", "") }
            /^[ 	]*(UNIX_)?SRC[ 	]*=.*/ {
                src = $2
                sub("^[ 	]*", "", src)
                while (sub("\\\\$", "", src)) {
                    print src
                    getline src
                    sub("^[ 	]*", "", src)
                }
                print src
            }' $x | fmt -w 1 | sort -u > "$cache_dir/$x"
        act=true
    fi
done

if [ $act = true ]; then
    for x in $files; do
        for y in $files; do
            if [ "$x" \!= "$y" ]  &&  \
               [ -n "`comm -12 \"$cache_dir/$x\" \"$cache_dir/$y\"`" ]; then
                echo "$x $common" | \
                    sed -e 's/^Makefile\.\(.*\)\.[la][ip][bp] /\1 /'
                break
            fi
        done
    done > "$lock_map.new"
    cmp -s "$lock_map" "$lock_map.new"  ||  mv -f "$lock_map.new" "$lock_map"
    # Don't bother keeping empty maps around.
    [ -s "$lock_map" ]  ||  rm "$lock_map"
fi

print_rdep_count()
{
    test -f Makefile.$1.$2  ||  return
    target=`awk "/\.$4\.real :/ { x=\\$1 } /cd $d;.* $3_PROJ=$1 / { print x; exit }" $mff`
    if [ -n "$target" ]; then
        echo `grep -c " $target" $mff` $target Makefile.$1.$2
    else
        echo "WARNING: couldn't find $rel_srcdir/$x in $mff" >&2
    fi
}

if [ "${rel_srcdir:-.}" != . -a -n "$builddir" -a \
     -w "$builddir/build-system" ]; then
    hintfile=$builddir/build-system/`echo "$rel_srcdir" | tr / _`.hints
    if [ -f "$lock_map" ]; then
        if $test "$hintfile" -nt "$lock_map"; then
            exit 0 # Already up to date
        fi
        exec > "$hintfile.$$"
cat <<EOF
ifneq "" "\$(wildcard \$(top_srcdir)/src/$rel_srcdir/flat-hints.mk)"
  include \$(top_srcdir)/src/$rel_srcdir/flat-hints.mk
else
EOF
        d=`echo $rel_srcdir/ | sed -e 's,/,\\\\/,g'`
        mff=$builddir/Makefile.flat
        set _
        shift
        while read x y; do
            print_rdep_count $x app APP exe
            print_rdep_count $x lib LIB '(lib|dll)'
        done < "$lock_map" | \
        sort -rn | \
        while read n x mf; do
            v=`echo $x | tr .- __`
            echo "  ifneq '' '\$(wildcard \$(top_srcdir)/src/$rel_srcdir/$mf)'"
            echo "    $v = $x"
            [ $# = 0 ]  ||  echo "    $x: $*"
            echo "  endif"
            set "$@" "\$($v)"
        done
        echo 'endif'
        mv "$hintfile.$$" "$hintfile"
    else
        rm -f "$hintfile"
    fi
fi

exit 0
