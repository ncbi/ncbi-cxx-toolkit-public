# $Id$

APP = dbapi_context_test
SRC = dbapi_unit_test_common dbapi_context_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = dbapi$(STATIC) dbapi_driver$(STATIC) \
       $(COMPRESS_LIBS) $(XCONNEXT) test_boost$(STATIC) xconnect xutil xncbi
STATIC_LIB = $(DBAPI_CTLIB) $(DBAPI_MYSQL) $(DBAPI_ODBC) $(DBAPI_SQLITE3) \
             ncbi_xdbapi_ftds $(FTDS_LIB) ncbi_xdbapi_ftds8 $(FTDS8_LIB) \
             ncbi_xdbapi_ftds_odbc $(FTDS64_ODBC_LIB) $(LIB)

PRE_LIBS = $(BOOST_LIBS)
LIBS =  $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
STATIC_LIBS = $(SYBASE_LIBS) $(SYBASE_DLLS) $(ODBC_LIBS) $(SQLITE3_LIBS) \
              $(MYSQL_LIBS) $(Z_LIBS) $(FTDS_LIBS) $(FTDS8_LIBS) \
              $(FTDS64_ODBC_LIBS) $(LIBS)

REQUIRES = Boost.Test

CHECK_COPY = dbapi_context_test.ini

CHECK_TIMEOUT = 60

CHECK_CMD = dbapi_context_test -d ctlib      -S MsSql
CHECK_CMD = dbapi_context_test -d dblib      -S MsSql
CHECK_CMD = dbapi_context_test -d ftds       -S MsSql
CHECK_CMD = dbapi_context_test -d odbc       -S MsSql
CHECK_CMD = dbapi_context_test -d ftds_odbc  -S MsSql
CHECK_CMD = dbapi_context_test -d ftds_dblib -S MsSql
CHECK_CMD = dbapi_context_test -d ftds8      -S MsSql
CHECK_CMD = dbapi_context_test -d ctlib      -S Sybase
CHECK_CMD = dbapi_context_test -d dblib      -S Sybase
CHECK_CMD = dbapi_context_test -d ftds       -S Sybase
CHECK_CMD = dbapi_context_test -d odbc       -S Sybase
CHECK_CMD = dbapi_context_test -d ftds_odbc  -S Sybase
CHECK_CMD = dbapi_context_test -d ftds_dblib -S Sybase
CHECK_CMD = dbapi_context_test -d ftds8      -S Sybase
