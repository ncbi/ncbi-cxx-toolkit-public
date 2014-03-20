#! /bin/sh
#$Id$

GENBANK_LOADER_METHOD="$1"
export GENBANK_LOADER_METHOD
shift

if test "$GENBANK_LOADER_METHOD" = pubseqos; then
    # special checks and settings for PubSeqOS reader
    if test ! -f "../../../../status/PubSeqOS.enabled"; then
        echo Sybase is disabled or unaware of PubSeqOS: skipping PUBSEQOS loader test
        exit 0
    fi
    if grep "^signature = GCC_295-.*-linux" ../../../Makefile.mk > /dev/null 2>&1; then
        echo "This test is disabled for GCC-2.95 compiler on Linux"
        exit 0
    fi
    NCBI_LOAD_PLUGINS_FROM_DLLS=1
    export NCBI_LOAD_PLUGINS_FROM_DLLS
fi

NCBI_ABORT_ON_NULL=1
export NCBI_ABORT_ON_NULL

exitcode=0
for args in "" "-fromgi 30240900 -togi 30241000"; do
    echo "Testing: test_objmgr_data_mt $args"
    $CHECK_EXEC test_objmgr_data_mt $args
    error=$?
    if test $error -ne 0; then
        echo "test_objmgr_data_mt $args failed: $error"
        exitcode=$error
        case $error in
            # signal 1 (HUP), 2 (INTR), 9 (KILL), or 15 (TERM).
            129|130|137|143) echo "Apparently killed"; break ;;
        esac
    fi
done
exit $exitcode
