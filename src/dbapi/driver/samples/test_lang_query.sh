#! /bin/sh
# $Id$

# DBLIB does not work (on Linux at least) when this limit is > 1024
# CTLIB does not work on Solaris sparc when this limit is < ~1300
ulimit -n 1536 > /dev/null 2>&1


# driver_list="ftds64_dblib odbcw ftds63 ftds64_odbc"
driver_list="ctlib dblib ftds odbc msdblib ftds8"
# server_list="MS_DEV2 BARTOK BARTOK_12 MSSQL9 STRAUSS"
server_list="MS_DEV1 TAPER"
# server_mssql="MS_DEV2 MSSQL9"
server_mssql="MS_DEV1"

res_file="/tmp/test_lang_query.sh.$$"
trap 'rm -f $res_file' 1 2 15

n_ok=0
n_err=0
sum_list=""
driver_status=0
HOST_NAME=`(hostname || uname -n) 2>/dev/null | sed 1q`
SYSTEM_NAME=`uname -s`

## Get a processor type.
PROCESSOR_TYPE=`uname -p`

if test $PROCESSOR_TYPE = "unknown" ; then
    PROCESSOR_TYPE=`uname -m`
fi

PROCESSOR_TYPE=`echo $PROCESSOR_TYPE | sed -e 's/[0-9].*//'`

# Run one test
RunSimpleTest()
{
    # echo
    (
        cd $1 > /dev/null 2>&1
        # $CHECK_EXEC run_sybase_app.sh $cmd > $res_file 2>&1
        $CHECK_EXEC run_sybase_app.sh $cmd > /dev/null 2> $res_file
    )

    if test $? -eq 0 ; then
        # echo "OK:"
        n_ok=`expr $n_ok + 1`
        sum_list="$sum_list XXX_SEPARATOR +  $cmd"
        return
    fi

    # error occurred
    n_err=`expr $n_err + 1`
    sum_list="$sum_list XXX_SEPARATOR -  $cmd"

    echo
    cat $res_file
}

# Run one test (RunTest sql_command reg_expression)
RunTest()
{
    sql="$1"
    reg_exp="$2"
    
    echo
    $CHECK_EXEC run_sybase_app.sh $cmd "$sql" > $res_file 2>&1
    
    if test $? -eq 0 ; then
        if grep "$reg_exp" $res_file > /dev/null 2>&1 ; then
        echo "OK:"
        grep "$reg_exp" $res_file
        n_ok=`expr $n_ok + 1`
        sum_list="$sum_list XXX_SEPARATOR +  $cmd '$sql'"
        return
        fi
    fi

    # error occurred
    n_err=`expr $n_err + 1`
    sum_list="$sum_list XXX_SEPARATOR -  $cmd '$sql'"

    cat $res_file
}

# Run one test (RunTest sql_command)
RunTest2()
{
    sql="$1"

    # echo
    $CHECK_EXEC run_sybase_app.sh $cmd "$sql" > /dev/null 2> $res_file 

    if test $? -eq 0 ; then
        # echo "OK:"
        n_ok=`expr $n_ok + 1`
        sum_list="$sum_list XXX_SEPARATOR +  $cmd '$sql'"
        return
    fi

    # error occurred
    n_err=`expr $n_err + 1`
    sum_list="$sum_list XXX_SEPARATOR -  $cmd '$sql'"

    echo
    cat $res_file
}


# Check existence of the "dbapi_driver_check"
$CHECK_EXEC run_sybase_app.sh dbapi_driver_check
if test $? -ne 99 ; then
    echo "The DBAPI driver existence check application not found."
    echo
    exit 1
fi


# Loop through all combinations of {driver, server, test}
for driver in $driver_list ; do
    cat <<EOF

******************* DRIVER:  $driver ************************
EOF


    $CHECK_EXEC run_sybase_app.sh dbapi_driver_check $driver
    driver_status=$?

    if test $driver_status -eq 5; then 
        cat <<EOF

Driver not found.
EOF
    elif test $driver_status -eq 4; then
        n_err=`expr $n_err + 1`
        sum_list="$sum_list XXX_SEPARATOR -  dbapi_driver_check $driver (Database-related driver initialization error)"
        cat <<EOF
    
Database-related driver initialization error.
EOF
    elif test $driver_status -eq 3; then
        n_err=`expr $n_err + 1`
        sum_list="$sum_list XXX_SEPARATOR -  dbapi_driver_check $driver (Corelib-related driver initialization error)"
        cat <<EOF

Corelib-related driver initialization error.
EOF
    elif test $driver_status -eq 2; then
        n_err=`expr $n_err + 1`
        sum_list="$sum_list XXX_SEPARATOR -  dbapi_driver_check $driver (C++-related driver initialization error)"
        cat <<EOF

C++-related driver initialization error.
EOF
    elif test $driver_status -eq 1; then
        n_err=`expr $n_err + 1`
        sum_list="$sum_list XXX_SEPARATOR -  dbapi_driver_check $driver (Unknown driver initialization error)"
        cat <<EOF

Unknown driver initialization error.
EOF

    else
        for server in $server_list ; do
            if test $driver = "ctlib"  -a  $server = $server_mssql ; then
                continue
            fi
            if test \( $driver = "ftds64" -o $driver = "odbc" -o $driver = "msdblib" \) -a  $server != $server_mssql ; then
                continue
            fi

            cat <<EOF

~~~~~~ SERVER:  $server ~~~~~~~~~~~~~~~~~~~~~~~~
EOF

        # lang_query

            cmd="lang_query -lb random -d $driver -S $server -Q"
            if test $driver != "ftds63" ; then
                RunTest2 'select qq = 57.55 + 0.0033' '<ROW><qq>57\.5533<'
            else
                sum_list="$sum_list XXX_SEPARATOR #  $cmd select qq = 57.55 + 0.0033 (skipped)"
            fi
            RunTest2 'select qq = 57 + 33' '<ROW><qq>90<'
            RunTest2 'select qq = GETDATE()' '<ROW><qq>../../.... ..:..:..<'
            RunTest2 'select name, type from sysobjects' '<ROW><name>'

        # simple tests

            # dblib is not  supposed to work with MS SQL Server
            if test $driver = "dblib"  -a  $server = $server_mssql ; then
                continue
            fi

            cmd="dbapi_conn_policy -lb random -d $driver -S $server"
            RunSimpleTest "dbapi_conn_policy"

            # Do not run dbapi_bcp and dbapi_testspeed on SunOS Intel
            if test $driver = "ctlib" -a \( $SYSTEM_NAME = "SunOS" -a $PROCESSOR_TYPE = "i" \) ; then
                sum_list="$sum_list XXX_SEPARATOR #  dbapi_bcp -lb random -d $driver -S $server (skipped because of invalid Sybase client installation)"
                sum_list="$sum_list XXX_SEPARATOR #  dbapi_testspeed -lb random -d $driver -S $server (skipped because of invalid Sybase client installation)"
#            elif test $driver = "ftds8" -a  $server != $server_mssql ; then
#                sum_list="$sum_list XXX_SEPARATOR #  dbapi_bcp -lb random -d $driver -S $server (skipped)"
#                sum_list="$sum_list XXX_SEPARATOR #  dbapi_testspeed -lb random -d $driver -S $server (skipped)"
            else
                # do not run tests with a boolk copy operations 
                # on Sybase databases with the "ftds" driver
                if test \( $driver = "ftds" -o $driver = "ftds8" \) -a  $server != $server_mssql ; then
                    sum_list="$sum_list XXX_SEPARATOR #  dbapi_bcp -lb random -d $driver -S $server (skipped)"
                else
                    cmd="dbapi_bcp -lb random -d $driver -S $server"
                    RunSimpleTest "dbapi_bcp"
                fi

                cmd="dbapi_testspeed -lb random -d $driver -S $server"
                RunSimpleTest "dbapi_testspeed"
            fi

            # Do not run dbapi_cursor on SunOS Intel
            cmd="dbapi_cursor -lb random -d $driver -S $server"
            if test $driver = "ctlib" -a \( $SYSTEM_NAME = "SunOS" -a $PROCESSOR_TYPE = "i" \) ; then
                sum_list="$sum_list XXX_SEPARATOR #  $cmd (skipped because of invalid Sybase client installation)"
            elif test \( $driver = "ftds8" -o $driver = "ftds" -o $driver = "msdblib" \) -a  $server != $server_mssql ; then
                sum_list="$sum_list XXX_SEPARATOR #  $cmd (skipped)"
            else
                RunSimpleTest "dbapi_cursor"
            fi


            cmd="dbapi_query -lb random -d $driver -S $server"
            RunSimpleTest "dbapi_query"

            cmd="dbapi_send_data -lb random -d $driver -S $server"
            RunSimpleTest "dbapi_send_data"

        done

    fi

done

rm -f $res_file


# Print summary
cat <<EOF


*******************************************************

SUCCEEDED:  $n_ok
FAILED:     $n_err
EOF
echo "$sum_list" | sed 's/XXX_SEPARATOR/\
/g'


# Exit
exit $n_err
