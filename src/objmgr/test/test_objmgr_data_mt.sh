#! /bin/sh
#$Id$

for args in "" "-fromgi 30240900 -togi 30241000"; do
    echo "Testing: ./test_objmgr_data_mt $args -repeats 4"
    if $CHECK_EXEC time ./test_objmgr_data_mt $args -repeats 4 ; then
        echo "Done."
    else
        exit 1
    fi
done
exit 0
