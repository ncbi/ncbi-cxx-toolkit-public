#! /bin/sh
#$Id$

NCBI_ABORT_ON_NULL=1
export NCBI_ABORT_ON_NULL

for args in "" "-fromgi 30240900 -togi 30241000"; do
    echo "Testing: test_objmgr_data $args"
    if $CHECK_EXEC time test_objmgr_data $args; then
        echo "Done."
    else
        exit 1
    fi
done
exit 0
