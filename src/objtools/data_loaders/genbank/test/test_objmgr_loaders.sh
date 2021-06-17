#! /bin/sh
#$Id$

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

if disabled PubSeqOS; then
    echo "Skipping PUBSEQOS loader test (loader unavailable)"
    methods="ID1 ID2"
elif disabled in-house-resources; then
    echo "Skipping PUBSEQOS loader test (in-house resources unavailable)"
    methods="ID1 ID2"
else
    methods="PUBSEQOS ID1 ID2"
    NCBI_LOAD_PLUGINS_FROM_DLLS=1
    export NCBI_LOAD_PLUGINS_FROM_DLLS
fi

exitcode=0
for method in $methods; do
    echo "Checking GenBank loader $method:"
    GENBANK_LOADER_METHOD="$method"
    export GENBANK_LOADER_METHOD
    $CHECK_EXEC "$@"
    error=$?
    if test $error -ne 0; then
        echo "Test of GenBank loader $method failed: $error"
        exitcode=$error
        case $error in
            # signal 1 (HUP), 2 (INTR), 9 (KILL), or 15 (TERM).
            129|130|137|143) echo "Apparently killed"; break ;;
        esac
    fi
    unset NCBI_LOAD_PLUGINS_FROM_DLLS
done

exit $exitcode
