# $Id$

APP = dbapi_context_test
SRC = dbapi_unit_test_common dbapi_context_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = dbapi$(STATIC) dbapi_driver$(STATIC) \
       $(COMPRESS_LIBS) $(XCONNEXT) test_boost$(STATIC) xconnect xutil xncbi
STATIC_LIB = $(DBAPI_CTLIB) $(DBAPI_MYSQL) $(DBAPI_ODBC) \
             ncbi_xdbapi_ftds $(FTDS_LIB) $(LIB)

LIBS =  $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
STATIC_LIBS = $(SYBASE_LIBS) $(SYBASE_DLLS) $(ODBC_LIBS) \
              $(MYSQL_LIBS) $(Z_LIBS) $(FTDS_LIBS) $(LIBS)

REQUIRES = Boost.Test.Included

CHECK_REQUIRES = in-house-resources

CHECK_COPY = dbapi_context_test.ini

CHECK_TIMEOUT = 60

CHECK_CMD = dbapi_context_test -dr ftds       -S MsSql
CHECK_CMD = dbapi_context_test -dr odbc       -S MsSql
CHECK_CMD = dbapi_context_test -dr ctlib      -S Sybase
CHECK_CMD = dbapi_context_test -dr ftds       -S Sybase

WATCHERS = ucko
