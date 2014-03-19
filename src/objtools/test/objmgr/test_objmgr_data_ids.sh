#! /bin/sh
#$Id$

GENBANK_LOADER_METHOD="$1"
export GENBANK_LOADER_METHOD
shift

OBJMGR_SCOPE_AUTORELEASE_SIZE=0
export OBJMGR_SCOPE_AUTORELEASE_SIZE
OBJMGR_BLOB_CACHE=0
export OBJMGR_BLOB_CACHE
GENBANK_ID2_DEBUG=5
export GENBANK_ID2_DEBUG

for mode in "" "-no_reset" "-keep_handles" "-no_reset -keep_handles"; do
    for file in test_objmgr_data.id*; do
        echo "Testing: $@ $mode -idlist $file"
        if time $CHECK_EXEC "$@" $mode -idlist "$file"; then
            echo "Done."
        else
            exit 1
        fi
    done
done

exit 0
