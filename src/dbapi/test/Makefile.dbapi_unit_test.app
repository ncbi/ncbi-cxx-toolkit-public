# $Id$

APP = dbapi_unit_test
SRC = dbapi_unit_test dbapi_unit_test_object dbapi_unit_test_lob dbapi_unit_test_bcp \
      dbapi_unit_test_proc dbapi_unit_test_cursor dbapi_unit_test_stmt \
      dbapi_unit_test_connection dbapi_unit_test_timeout dbapi_unit_test_context \
      dbapi_unit_test_msg dbapi_unit_test_common

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = dbapi dbapi_driver dbapi_util_blobstore$(STATIC) \
       $(COMPRESS_LIBS) $(XCONNEXT) test_boost$(STATIC) xconnect xutil xncbi
PRE_LIBS = $(BOOST_LIBS)
LIBS =  $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test

CHECK_COPY = dbapi_unit_test.ini

CHECK_TIMEOUT = 600

CHECK_CMD = dbapi_unit_test -d ctlib      -S MsSql
CHECK_CMD = dbapi_unit_test -d dblib      -S MsSql
CHECK_CMD = dbapi_unit_test -d ftds       -S MsSql
CHECK_CMD = dbapi_unit_test -d odbc       -S MsSql
CHECK_CMD = dbapi_unit_test -d ftds_odbc  -S MsSql
CHECK_CMD = dbapi_unit_test -d ftds_dblib -S MsSql
CHECK_CMD = dbapi_unit_test -d ftds8      -S MsSql
CHECK_CMD = dbapi_unit_test -d ctlib      -S Sybase
CHECK_CMD = dbapi_unit_test -d dblib      -S Sybase
CHECK_CMD = dbapi_unit_test -d ftds       -S Sybase
CHECK_CMD = dbapi_unit_test -d odbc       -S Sybase
CHECK_CMD = dbapi_unit_test -d ftds_odbc  -S Sybase
CHECK_CMD = dbapi_unit_test -d ftds_dblib -S Sybase
CHECK_CMD = dbapi_unit_test -d ftds8      -S Sybase
