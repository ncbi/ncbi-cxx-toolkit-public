# $Id$

APP = dbapi_unit_test
SRC = dbapi_unit_test dbapi_unit_test_object dbapi_unit_test_lob dbapi_unit_test_bcp \
      dbapi_unit_test_proc dbapi_unit_test_cursor dbapi_unit_test_stmt \
      dbapi_unit_test_connection dbapi_unit_test_timeout dbapi_unit_test_context \
      dbapi_unit_test_msg dbapi_unit_test_common

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = ncbi_xdbapi_ftds
       ncbi_xdbapi_ftds100 $(FTDS100_LIB) $(DBAPI_CTLIB) $(DBAPI_ODBC) \
       dbapi$(STATIC) dbapi_util_blobstore$(STATIC) dbapi_driver$(STATIC) \
       $(XCONNEXT) xconnect $(COMPRESS_LIBS) xutil test_boost xncbi

LIBS = $(SYBASE_LIBS) $(SYBASE_DLLS) $(ODBC_LIBS) $(FTDS95_LIBS) \
       $(FTDS100_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_REQUIRES = connext in-house-resources

CHECK_COPY = dbapi_unit_test.ini namerd.ini

CHECK_TIMEOUT = 600

CHECK_CMD = dbapi_unit_test -dr ftds100    -S MsSql
CHECK_CMD = dbapi_unit_test -dr ftds100    -S MsSql -V 74
CHECK_CMD = dbapi_unit_test -dr odbc       -S MsSql --log_level=test_suite
CHECK_CMD = dbapi_unit_test -dr ctlib      -S Sybase
CHECK_CMD = dbapi_unit_test -dr ftds100    -S Sybase
CHECK_CMD = dbapi_unit_test -dr ctlib      -S DBAPI_DEV16_16K
CHECK_CMD = dbapi_unit_test -dr ftds100    -S DBAPI_DEV16_16K

# Test for successful NAMERD service name resolution by using a service name
# that should be resolved by NAMERD, but is not present
# in the interfaces file or DNS.
CHECK_CMD = dbapi_unit_test -dr ftds100    -S DBAPI_SYB160_TEST         -conffile namerd.ini

WATCHERS = ucko satskyse
