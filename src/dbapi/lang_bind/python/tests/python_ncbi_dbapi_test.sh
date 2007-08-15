#! /bin/sh
# $Id$

# Declare drivers and servers
# Tere are problems with ftds63 ...
driver_list="ctlib dblib ftds odbc odbcw msdblib ftds_odbc ftds8" # mysql
server_list="MS_DEV1 TAPER"
server_mssql="MS_DEV1"

# DBLIB does not work (on Linux at least) when this limit is > 1024
ulimit -n 1024 > /dev/null 2>&1

# Python module located in $CFG_LIB on *nix and in $CFG_BIN on Windows.
# PYTHONPATH="$CFG_LIB:$CFG_BIN"
if test -f $CFG_LIB/libpython_ncbi_dbapi.so ; then
    PYTHONPATH="$CFG_LIB"
    LD_LIBRARY_PATH="$CFG_LIB:$LD_LIBRARY_PATH"
    export LD_LIBRARY_PATH
else
    PYTHONPATH="$CFG_BIN"
fi

export PYTHONPATH

# We need this to find dbapi_driver_check
PATH="$CFG_BIN:$PATH"
export PATH

res_file="/tmp/python_ncbi_dbapi_test.sh.$$"
trap 'rm -f $res_file' 1 2 15

n_ok=0
n_err=0
sum_list=""
driver_status=0

# Run one test
RunTest()
{
  echo
  (
    $CHECK_EXEC run_sybase_app.sh python_ncbi_dbapi_test --detect_memory_leak=0 $1 > $res_file 2>&1
  )
  if test $? -eq 0 ; then
      echo "OK:"
      n_ok=`expr $n_ok + 1`
      sum_list="$sum_list XXX_SEPARATOR +  python_ncbi_dbapi_test $1"
      return
  fi

  # error occurred
  n_err=`expr $n_err + 1`
  sum_list="$sum_list XXX_SEPARATOR -  python_ncbi_dbapi_test $1"

  cat $res_file
}

# Check existence of the "dbapi_driver_check"
$CHECK_EXEC run_sybase_app.sh dbapi_driver_check
if test $? -ne 99 ; then
  echo "The DBAPI driver existence check application not found."
  echo
  exit 1
fi

# On Windows we already have a file with the correct name
if test -f $CFG_LIB/libpython_ncbi_dbapi.so ; then
    ln -sf $CFG_LIB/libpython_ncbi_dbapi.so $CFG_LIB/python_ncbi_dbapi.so
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
            if test \( $driver = "ctlib" -o $driver = "dblib" \) -a $server = $server_mssql ; then
                continue
            fi
            if test \( $driver = "odbc" -o $driver = "odbcw" -o $driver = "msdblib" -o $driver = "ftds_odbc" -o $driver = "ftds8" -o $driver = "ftds" \) -a  $server != $server_mssql ; then
                continue
            fi

            cat <<EOF

~~~~~~ SERVER:  $server ~~~~~~~~~~~~~~~~~~~~~~~~
EOF

            RunTest "-d $driver -S $server"
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
