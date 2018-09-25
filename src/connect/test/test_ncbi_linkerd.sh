#! /bin/sh
# $Id$

# Disable all Windows configurations.
#   The problem is that devops doesn't yet support linkerd on Windows.
#   When that support is added, delete this block and just disable
#   Windows DLL configurations in the next block.
if echo "$FEATURES" | grep -E '(^| )MSWin( |$)' > /dev/null; then
    echo "NCBI_UNITTEST_DISABLED"
    exit 0
fi

# Disable Windows DLL configurations.
#   The problem is that the program uses PARSON, which does not have the
#   dllexport/dllimport specifiers, resulting in link errors when built
#   with DLL on Windows.
if echo "$FEATURES" | grep -E '(^| )MSWin( |$)' > /dev/null  &&  \
   echo "$FEATURES" | grep -E '(^| )DLL( |$)' > /dev/null; then
    echo "NCBI_UNITTEST_DISABLED"
    exit 0
fi

# Set up prioritized list of locations in which to look for the test data file.
# 1. standard location used for CONNECT library test data
# 2. pwd
# 3. automount
if test -r ./ncbi_test_data; then
    . ./ncbi_test_data
    test_data_dirs=$NCBI_TEST_DATA/connect
fi
test_data_dirs="$test_data_dirs . /am/ncbiapdata/test_data/connect"

# Find a directory containing the test file.
test_file_name="test_ncbi_linkerd_tests.json"
for data_dir in $test_data_dirs; do
    if test ! -d $data_dir; then
        echo "INFO: Candidate test data dir not found: $data_dir"
        continue
    fi
    test_file="$data_dir/$test_file_name"
    if test ! -r $test_file; then
        echo "INFO: Candidate test data file not found: $test_file"
        continue
    fi
    break;
done
if test ! -r "$test_file"; then
    echo "ERROR: Test file '$test_file_name' not found in any standard test data directory ($test_data_dirs)."
    exit 1
fi

if test -n "$CHECK_EXEC"; then
    $CHECK_EXEC test_ncbi_linkerd -f $test_file
else
    ./test_ncbi_linkerd -f $test_file
fi
