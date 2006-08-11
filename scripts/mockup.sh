#!/bin/sh
# $Id$

if test "$#" -ne 2 -o \! -f "$1/build/Makefile.mk"; then
    echo "USAGE: $0 source_build_root fresh_destination" >&2
    exit 1
fi

set -e

src=$1
dest=$2
mkdir -p $dest
abs_dest=`cd $dest && pwd`

getvar() {
    sed -ne "s/^[ 	]*$1[ 	]*=[ 	]*\([+-~]*\).*/\1/p" $2
}

mkdir $abs_dest/status
cd $abs_dest/status
cp $src/status/*.enabled $src/status/.*.dep .
canon_src=`getvar build_root $src/build/Makefile.mk`
canon_top=`getvar top_srcdir $src/build/Makefile.mk`
sed -e "s:^\([real_]*srcdir\)=.*:\1=$canon_top:; s:$canon_src:$abs_dest:g" \
    $src/status/config.status > config.status
chmod +x config.status
rm -f src config.h.in
ln -s $canon_top/src src
ln -s $canon_top/config.h.in config.h.in
touch Makefile.in
./config.status

mkdir $abs_dest/lib
ln -s $src/lib/* $abs_dest/lib

mkdir $abs_dest/bin
ln -s $src/bin/*[^z] $abs_dest/bin
for x in $src/bin/*.gz; do
    base=`basename $x .gz`
    gunzip -cv $x > $abs_dest/bin/$base
    chmod +x $abs_dest/bin/$base
done

find $canon_top/src -name 'Makefile.*.app' | while read m; do
    dir=`dirname $m | sed -e "s:$canon_top/src/::"`
    revdir=`echo $dir/ | sed -e 's:[^/]*/:../:g'`
    app=`getvar APP $m`
    # echo "Symlinking $app into $dir" >&2
    # mkdir -p $abs_dest/build/$dir
    if test -f "$abs_dest/build/$dir/Makefile"; then
        ln -s ${revdir}../bin/$app $abs_dest/build/$dir/$app  ||  true
    else
        if test ! -f "$canon_top/src/$dir/Makefile.in"  || \
           cmp -s "$canon_top/src/$dir/Makefile.in" \
              "$canon_top/src/Makefile.in.skel"; then
            echo "Skipping $dir/$app -- no corresponding meta-makefile." >&2
        fi
    fi
done
