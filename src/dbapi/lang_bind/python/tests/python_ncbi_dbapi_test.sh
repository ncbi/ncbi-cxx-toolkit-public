#! /bin/sh
# $Id$

# DBLIB does not work (on Linux at least) when this limit is > 1024
ulimit -n 1024 > /dev/null 2>&1

# Python module located in $CFG_LIB on *nix and in $CFG_BIN on Windows.
# PYTHONPATH="$CFG_LIB:$CFG_BIN"
if test -f $CFG_LIB/libpython_ncbi_dbapi.so ; then
    PYTHONPATH="$CFG_LIB"
else
    PYTHONPATH="$CFG_BIN"
fi

export PYTHONPATH

# We need this to find dbapi_driver_check
PATH="$CFG_BIN:$PATH"
export PATH

# Check existence of the "dbapi_driver_check"
$CHECK_EXEC dbapi_driver_check
if test $? -ne 99 ; then
  echo "The DBAPI driver existence check application not found."
  echo
  exit 1
fi

# On Windows we already have a file with the correct name
if test -f $CFG_LIB/libpython_ncbi_dbapi.so ; then
    ln -sf $CFG_LIB/libpython_ncbi_dbapi.so $CFG_LIB/python_ncbi_dbapi.so
fi

if $CHECK_EXEC dbapi_driver_check ftds ; then
    $CHECK_EXEC python_ncbi_dbapi_test
    exit $?
fi

exit 1