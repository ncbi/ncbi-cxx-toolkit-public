#! /bin/sh
# $Id$

# Factor out OS- and invocation-specific stuff.
if test "$OSTYPE" = "cygwin"; then
  bases=". //snowman/toolkit_test_data/connect"
else
  bases=". /net/snowman/vol/projects/toolkit_test_data/connect"
fi
if test -z "$CHECK_EXEC"; then
  tester="./test_ncbi_namerd"
else
  tester="test_ncbi_namerd"
fi

# Find a directory containing the test file.
for base in $bases; do
    if test ! -d $base; then
        echo "Test data dir not found: $base"
        continue
    fi
    test_file="$base/test_ncbi_namerd_tests.json"
    if test ! -r $test_file; then
        echo "Test file not found: $test_file"
        continue
    fi
    break;
done

if test ! -r "$test_file"; then
    echo "ERROR: Test file not found in any base directory."
    exit 1
fi

$CHECK_EXEC $tester -f $test_file
