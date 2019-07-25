#!/bin/sh
srcdir=`(cd $1 && pwd)`
builddir=$2
find "$srcdir"/src/* -name .svn -prune -o -name build-system -prune \
   -o -name 'Makefile.*.mk' -print \
   | while read x; do
      echo "\$(builddir)/Makefile.mk: \$(wildcard $x)" >&3
      echo
      echo "### Extra macro definitions from $x"
      echo
      echo "#line 1 \"$x\""
      cat "$x"
   done >> "$builddir/Makefile.mk" 3>"$builddir/Makefile.mk.d"
"$srcdir"/scripts/common/impl/report_duplicates.awk \
   src="$srcdir/src/build-system/Makefile.mk.in" "$builddir/Makefile.mk"
