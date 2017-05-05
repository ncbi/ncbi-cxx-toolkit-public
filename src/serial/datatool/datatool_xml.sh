#! /bin/sh
# $Id$
#

bases="./testdata ${NCBI_TEST_DATA}/objects/datatool"
if test -z "$CHECK_EXEC"; then
  tool="./datatool"
else
  tool="datatool"
fi

for base in $bases; do
  if test ! -d $base; then
    echo "Error -- test data dir not found: $base"
    echo "Test will be skipped."
    echo " (for autobuild scripts: NCBI_UNITTEST_SKIPPED)"
    continue
  fi

  d="$base/data"
  r="$base/res"

  for i in "idx" "elink" "note" "alltest"; do
    spec=`ls $base/$i.*`    
    if ! test -e "$spec"; then
        echo "file not found: $base/$i"
        continue
    fi
    echo "$tool" -m "$spec" -vx "$d/$i.xml" -px out
    cmd=`echo "$tool" -m "$spec" -vx "$d/$i.xml" -px out`
    $CHECK_EXEC $cmd
    if test "$?" != 0; then
        echo "datatool failed!"
        exit 1
    fi
    diff -w out "$r/$i.xml"
    if test "$?" != 0; then
        echo "wrong result!"
        exit 1
    fi
  done

  echo "Done!"
done
