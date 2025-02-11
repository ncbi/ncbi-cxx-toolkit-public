#! /bin/sh
#$Id$

GENBANK_LOADER_METHOD="$1"
export GENBANK_LOADER_METHOD
shift
if test "$GENBANK_LOADER_METHOD" = psg; then
    GENBANK_LOADER_PSG=1
else
    if test "$ONLY_PSG" = 1; then
        echo "Skipping non-PSG test because ONLY_PSG=1"
	exit
    fi
    GENBANK_LOADER_PSG=0
fi
export GENBANK_LOADER_PSG

status_dir="$CFG_LIB/../status"
if test ! -d "$status_dir"; then
    status_dir="../../../../status"
fi

disabled() {
    if test -f "$status_dir/$1.enabled"; then
        return 1
    fi
    case "$FEATURES" in
        *" $1 "*) return 1;;
    esac
    return 0;
}

if test "$GENBANK_LOADER_METHOD" = pubseqos; then
    # special checks and settings for PubSeqOS reader
    if disabled PubSeqOS; then
        echo "Skipping PUBSEQOS loader test (loader unavailable)"
        exit 0
    elif disabled in-house-resources; then
        echo "Skipping PUBSEQOS loader test (in-house resources unavailable)"
        exit 0
    fi
    if grep "^signature = GCC_295-.*-linux" ../../../Makefile.mk > /dev/null 2>&1; then
        echo "This test is disabled for GCC-2.95 compiler on Linux"
        exit 0
    fi
    NCBI_LOAD_PLUGINS_FROM_DLLS=1
    export NCBI_LOAD_PLUGINS_FROM_DLLS
fi

if test "$GENBANK_LOADER_METHOD" = psg; then
    # special check for PSG loader availability
    if disabled PSGLoader || disabled in-house-resources; then
        echo PSG loader is disabled or not accessible: skipping PSG loader test
        exit 0
    fi
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
