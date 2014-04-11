#! /bin/sh
# $Id$

# DBLIB does not work (on Linux at least) when this limit is > 1024
# CTLIB does not work on Solaris sparc when this limit is < ~1300
ulimit -n 1536 > /dev/null 2>&1

# Clear any locale settings that might lead to trouble (observed with
# ctlib against DBAPI_SYB155_TEST)
unset LANG LC_ALL LC_CTYPE

driver_list="ctlib dblib ftds odbc"

if echo $FEATURES | grep "\-connext" > /dev/null ; then
    server_list="MSDEV1 DBAPI_DEV3"
    server_mssql="MSDEV1"

    server_mssql2005="MSDEV1"
else
    # server_list="MS_DEV2"
    server_list="DBAPI_MS_TEST DBAPI_SYB155_TEST"
    # server_mssql="MS_DEV2"
    server_mssql="DBAPI_MS_TEST"

    server_mssql2005="DBAPI_MS_TEST"
fi

if echo $FEATURES | grep "DLL" > /dev/null ; then
    static_config=0
else
    static_config=1
fi

if echo $FEATURES | grep "MSWin" > /dev/null ; then
    win_config=1
else
    win_config=0
fi

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
        # run_sybase_app.sh $cmd > $res_file 2>&1
        run_sybase_app.sh $cmd > /dev/null 2> $res_file
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
    run_sybase_app.sh $cmd "$sql" > $res_file 2>&1
    
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
    run_sybase_app.sh $cmd "$sql" > /dev/null 2> $res_file 

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
run_sybase_app.sh dbapi_driver_check
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


    run_sybase_app.sh dbapi_driver_check $driver
    driver_status=$?

    if test $driver_status -eq 5 -a \( $static_config -eq 0 -o $win_config -eq 0 \); then 
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
            if test \( $driver = "ctlib" -o $driver = "dblib" \)  -a  \( $server = $server_mssql -o $server = $server_mssql2005 -o \( $static_config = 1 -a $win_config = 1 \) \) ; then
                continue
            fi

            if test \( $driver = "ftds64" -o $driver = "odbc" \) -a  $server != $server_mssql -a  $server != $server_mssql2005 ; then
                continue
            fi
             
            if test \( -z "$SYBASE" -o "$SYBASE" = "No_Sybase" \) -a -f "/netopt/Sybase/clients/current/interfaces" ; then
                SYBASE="/netopt/Sybase/clients/current"
                export SYBASE
            fi

            cat <<EOF

~~~~~~ SERVER:  $server ~~~~~~~~~~~~~~~~~~~~~~~~
EOF

        # lang_query

            cmd="lang_query -lb on -d $driver -S $server -Q"
            if test $driver != 'dblib' -o \( $driver = 'dblib' -a $server != $server_mssql -a $server != $server_mssql2005 \) ; then
                RunTest2 'select qq = 57.55 + 0.0033' '<ROW><qq>57\.5533<'
            fi

            RunTest2 'select qq = convert(real, 57.55 + 0.0033)' '<ROW><qq>57\.5533<'
            RunTest2 'select qq = 57 + 33' '<ROW><qq>90<'
            RunTest2 'select qq = GETDATE()' '<ROW><qq>../../.... ..:..:..<'
            RunTest2 'select name, type from sysobjects' '<ROW><name>'

        # simple tests

            # dblib is not  supposed to work with MS SQL Server
            if test $driver = "dblib"  -a  \( $server = $server_mssql -o $server = $server_mssql2005 \) ; then
                continue
            fi

            cmd="dbapi_conn_policy -lb on -d $driver -S $server"
            RunSimpleTest "dbapi_conn_policy"

            # Do not run dbapi_bcp and dbapi_testspeed on SunOS Intel
            if test $driver = "ctlib" -a \( $SYSTEM_NAME = "SunOS" -a $PROCESSOR_TYPE = "i" \) ; then
                sum_list="$sum_list XXX_SEPARATOR #  dbapi_bcp -lb on -d $driver -S $server (skipped because of invalid Sybase client installation)"
                sum_list="$sum_list XXX_SEPARATOR #  dbapi_testspeed -lb on -d $driver -S $server (skipped because of invalid Sybase client installation)"
            else
                # do not run tests with a boolk copy operations 
                # on Sybase databases with the "ftds" driver
                if test $driver = "ftds" -a  $server != $server_mssql -a  $server != $server_mssql2005 ; then
                    sum_list="$sum_list XXX_SEPARATOR #  dbapi_bcp -lb on -d $driver -S $server (skipped)"
                else
                    cmd="dbapi_bcp -lb on -d $driver -S $server"
                    RunSimpleTest "dbapi_bcp"
                fi

                # do not run dbapi_testspeed 
                # on Sybase databases with the "ftds" driver
                if test \( $driver = "ftds" \) -a  $server != $server_mssql -a  $server != $server_mssql2005 ; then
                    sum_list="$sum_list XXX_SEPARATOR #  dbapi_testspeed -lb on -d $driver -S $server (skipped)"
                else
                    cmd="dbapi_testspeed -lb on -d $driver -S $server"
                    RunSimpleTest "dbapi_testspeed"
                fi
            fi

            # Do not run dbapi_cursor on SunOS Intel
            cmd="dbapi_cursor -lb on -d $driver -S $server"
            if test $driver = "ctlib" -a \( $SYSTEM_NAME = "SunOS" -a $PROCESSOR_TYPE = "i" \) ; then
                sum_list="$sum_list XXX_SEPARATOR #  $cmd (skipped because of invalid Sybase client installation)"
            elif test $driver = "ftds" -a  $server != $server_mssql -a  $server != $server_mssql2005 ; then
                sum_list="$sum_list XXX_SEPARATOR #  $cmd (skipped)"
            else
                RunSimpleTest "dbapi_cursor"
            fi


            cmd="dbapi_query -lb on -d $driver -S $server"
            RunSimpleTest "dbapi_query"

            cmd="dbapi_send_data -lb on -d $driver -S $server"
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
