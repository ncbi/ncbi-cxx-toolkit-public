#! /bin/sh
#$Id$

method="PUBSEQOS"

if test ! -f "../../../../status/Sybase.enabled"; then
    echo Sybase is disabled: skipping PUBSEQOS loader test
    exit 0
fi

if grep "^signature = GCC_295-.*-linux" ../../../Makefile.mk > /dev/null 2>&1; then
    echo "This test is disabled for GCC-2.95 compiler on Linux"
    exit 0
fi

echo "Checking GenBank loader $method:"
GENBANK_LOADER_METHOD="$method"
export GENBANK_LOADER_METHOD
$CHECK_EXEC "$@"
exitcode=$?
if test $exitcode -ne 0; then
    echo "Test of GenBank loader $method failed: $exitcode"
    case $exitcode in
        # signal 1 (HUP), 2 (INTR), 9 (KILL), or 15 (TERM).
        129|130|137|143) echo "Apparently killed"; break ;;
    esac
fi

exit $exitcode
