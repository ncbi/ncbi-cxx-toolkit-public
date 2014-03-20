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

OBJMGR_SCOPE_AUTORELEASE_SIZE=0
export OBJMGR_SCOPE_AUTORELEASE_SIZE
OBJMGR_BLOB_CACHE=0
export OBJMGR_BLOB_CACHE
GENBANK_ID2_DEBUG=5
export GENBANK_ID2_DEBUG

exitcode=0
for mode in "" "-no_reset" "-keep_handles" "-no_reset -keep_handles"; do
    for file in test_objmgr_data.id*; do
        echo "Testing: $@ $mode -idlist $file"
        $CHECK_EXEC "$@" $mode -idlist "$file"
        error=$?
        if test $error -ne 0; then
            echo "$@ $mode -idlist $file failed: $error"
            exitcode=$error
            case $error in
                # signal 1 (HUP), 2 (INTR), 9 (KILL), or 15 (TERM).
                129|130|137|143) echo "Apparently killed"; break 2 ;;
            esac
        fi
    done
done

exit $exitcode
