#! /bin/sh
# $Id$

# Disable Windows DLL configurations.
if echo "$FEATURES" | grep "MSWin" > /dev/null  &&  echo "$FEATURES" | grep "DLL" > /dev/null; then
    echo "NCBI_UNITTEST_DISABLED"
    exit 0
fi

# Use standard location, if operating from inside automated build.
if test -r ./ncbi_test_data; then
    . ./ncbi_test_data
    base_std=$NCBI_TEST_DATA/connect
fi
# Use pwd for direct invocations.
bases=". $base_std"

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

if test -n "$CHECK_EXEC"; then
    $CHECK_EXEC test_ncbi_namerd -f $test_file
else
    ./test_ncbi_namerd -f $test_file
fi
