#! /bin/sh
# $Id$

# DBLIB does not work (on Linux at least) when this limit is > 1024
ulimit -n 1024 > /dev/null 2>&1

# Declare drivers and servers
driver_list="ctlib dblib ftds ftds63 odbc odbcw msdblib ftds_odbc ftds_dblib ftds8" # mysql
server_list="MS_DEV1 SCHUMANN MSSQL67"
# "MSSQL67"
server_mssql="MS_DEV1"
server_mssql2005="MSSQL67"

res_file="/tmp/dbapi_unit_test.sh.$$"
trap 'rm -f $res_file' 1 2 15

n_ok=0
n_err=0
sum_list=""
driver_status=0

# We need this to find dbapi_driver_check
PATH="$CFG_BIN:$PATH"
export PATH

SYSTEM_NAME=`uname -s`
SYSTEM_MAJOR_RELEASE=`uname -r | sed -e 's/\..*//'`

BOOST_TEST_DETECT_MEMORY_LEAK=0
export BOOST_TEST_DETECT_MEMORY_LEAK

# Run one test
RunTest()
{
  echo
  (
    $CHECK_EXEC run_sybase_app.sh dbapi_unit_test $1 > $res_file 2>&1
  )
  if test $? -eq 0 ; then
      echo "OK:"
      n_ok=`expr $n_ok + 1`
      sum_list="$sum_list XXX_SEPARATOR +  dbapi_unit_test $1"
      return
  fi

  # error occurred
  n_err=`expr $n_err + 1`
  sum_list="$sum_list XXX_SEPARATOR -  dbapi_unit_test $1"

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
            if test \( -z "$SYBASE" -o "$SYBASE" = "No_Sybase" \) -a -f "/netopt/Sybase/clients/current/interfaces" ; then
                SYBASE="/netopt/Sybase/clients/current"
                export SYBASE
            fi

            if test $driver = "ctlib" -a \( $server = $server_mssql -o $server = $server_mssql2005 \) ; then
                continue
            fi

            if test $driver = "ftds_dblib" -a $server = $server_mssql2005 ; then
                continue
            fi

            if test \( $driver = "ftds_odbc" -o $driver = "odbc" -o $driver = "msdblib" \) -a  $server != $server_mssql -a  $server != $server_mssql2005 ; then
                continue
            fi

            ## Do not test odbc driver on FreeBSD 4.x. 
            if test \( $driver = "ftds_odbc" -o $driver = "odbc" \) -a $SYSTEM_NAME = "FreeBSD" -a $SYSTEM_MAJOR_RELEASE = 4 ; then
                sum_list="$sum_list XXX_SEPARATOR #  dbapi_unit_test -d $driver -S $server (skipped because of problems with a host name to IP resolution)"
                continue
            fi

            if test $driver = "ftds63" ; then
                sum_list="$sum_list XXX_SEPARATOR #  dbapi_unit_test -d $driver -S $server (skipped)"
                continue
            fi


            cat <<EOF

~~~~~~ SERVER:  $server ~~~~~~~~~~~~~~~~~~~~~~~~
EOF

        RunTest "-d $driver -S $server"
        RunTest "-d $driver -S $server -conf without-exceptions"
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
