#! /bin/sh
#$Id$

OBJMGR_SCOPE_AUTORELEASE_SIZE=0
export OBJMGR_SCOPE_AUTORELEASE_SIZE
OBJMGR_BLOB_CACHE=0
export OBJMGR_BLOB_CACHE

for mode in "" "-no_reset" "-keep_handles" "-no_reset -keep_handles"; do
    for file in test_objmgr_data.id*; do
        echo "Testing: $@ $mode -idlist $file"
        if $CHECK_EXEC time $@ $mode -idlist "$file"; then
            echo "Done."
        else
            exit 1
        fi
    done
done

exit 0
