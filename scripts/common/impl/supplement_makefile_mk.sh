#!/bin/sh
srcdir=`(cd $1 && pwd)`
builddir=$2
pattern='[ 	]*REQUIRES[ 	]*=' # Each [...] has a space and a tab
find "$srcdir"/src/* -name .svn -prune -o -name build-system -prune \
   -o -name 'Makefile.*.mk' -print \
   | while read x; do
      echo "\$(builddir)/Makefile.mk: \$(wildcard $x)" >&3
      unmet=''
      for r in `sed -ne "s/^$pattern//p" $x`; do
         [ -f $builddir/../status/$r.enabled ]  ||  unmet="$unmet $r"
      done
      if [ -n "$unmet" ]; then
         echo
         echo "### Skipping $x per unmet requirement(s):$unmet"
         echo
         continue
      fi
      echo
      echo "### Extra macro definitions from $x"
      echo
      echo "#line 1 \"$x\""
      sed -e "s/^\($pattern\)/# \\1/" "$x"
   done >> "$builddir/Makefile.mk" 3>"$builddir/Makefile.mk.d"
"$srcdir"/scripts/common/impl/report_duplicates.awk \
   src="$srcdir/src/build-system/Makefile.mk.in" "$builddir/Makefile.mk"
