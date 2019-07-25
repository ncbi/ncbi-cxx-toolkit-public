#!/bin/sh
srcdir=$1
builddir=$2
find "$srcdir"/src/* -name .svn -prune -o -name build-system -prune \
   -o -name 'Makefile.*.mk' -print \
   | while read x; do
      echo
      echo "### Extra macro definitions from $x"
      echo
      echo "#line 1 \"$x\""
      cat "$x"
   done >> "$builddir/Makefile.mk"
"$srcdir"/scripts/common/impl/report_duplicates.awk \
   src="$srcdir/src/build-system/Makefile.mk.in" "$builddir/Makefile.mk"
