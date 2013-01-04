#! /bin/sh
# $Id$
#

if test "$OSTYPE" = "cygwin"; then
  bases="./testdata //snowman/toolkit_test_data/objects/datatool"
else
  bases="./testdata /net/snowman/vol/projects/toolkit_test_data/objects/datatool"
fi
tool="./datatool"

for base in $bases; do
  if test ! -d $base; then
    echo "Error -- test data dir not found: $base"
    echo "Test will be skipped."
    echo " (for autobuild scripts: NCBI_UNITTEST_SKIPPED)"
    continue
  fi

  d="$base/data"
  r="$base/res"

  for i in "idx" "elink" "note"; do
    echo "$tool" -m "$base/$i.dtd" -vx "$d/$i.xml" -px out
    cmd=`echo "$tool" -m "$base/$i.dtd" -vx "$d/$i.xml" -px out`
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
