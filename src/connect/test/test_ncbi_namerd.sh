#! /bin/sh
# $Id$

# Use standard location, if operating from inside automated build.
if test -r ./ncbi_test_data; then
    . ./ncbi_test_data
    base_std=$NCBI_TEST_DATA/connect
fi

# Factor out OS- and invocation-specific stuff.
if test "$OSTYPE" = "cygwin"; then
  bases=". $base_std //snowman/toolkit_test_data/connect"
else
  bases=". $base_std /net/snowman/vol/projects/toolkit_test_data/connect"
fi

# Find a directory containing the test file.
test_file_name="test_ncbi_namerd_tests.json"
for base in $bases; do
    if test ! -d $base; then
        echo "INFO: Candidate test data dir not found: $base"
        continue
    fi
    test_file="$base/$test_file_name"
    if test ! -r $test_file; then
        echo "INFO: Candidate test data file not found: $test_file"
        continue
    fi
    break;
done
if test ! -r "$test_file"; then
    echo "ERROR: Test file '$test_file_name' not found in any base directory ($bases)."
    exit 1
fi

$CHECK_EXEC test_ncbi_namerd -f $test_file
