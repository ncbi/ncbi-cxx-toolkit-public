#! /bin/sh
#$Id$

status_dir="../../../status"

if test -f "$status_dir/Sybase.enabled"; then
    methods="PUBSEQOS ID1"
else
    echo Sybase is disabled: skipping PUBSEQOS loader test
    methods="ID1"
fi

exitcode=0
for method in $methods; do
    echo "Checking GenBank loader $method:"
    GENBANK_LOADER_METHOD="$method"
    export GENBANK_LOADER_METHOD
    "$@" &
    pid=$!
    trap 'kill $pid' 1 2 15
    wait $pid > /dev/null 2>&1
    error=$?
    if test $error -ne 0; then
        echo "Test of GenBank loader $method failed: $error"
        exitcode=$error
    fi
done

exit $exitcode
