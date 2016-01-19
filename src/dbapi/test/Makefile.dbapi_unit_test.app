# $Id$

APP = dbapi_unit_test
SRC = dbapi_unit_test dbapi_unit_test_object dbapi_unit_test_lob dbapi_unit_test_bcp \
      dbapi_unit_test_proc dbapi_unit_test_cursor dbapi_unit_test_stmt \
      dbapi_unit_test_connection dbapi_unit_test_timeout dbapi_unit_test_context \
      dbapi_unit_test_msg dbapi_unit_test_common

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = ncbi_xdbapi_ftds ncbi_xdbapi_ftds64 $(FTDS64_CTLIB_LIB) \
       ncbi_xdbapi_ftds95 $(FTDS95_LIB) $(DBAPI_CTLIB) $(DBAPI_ODBC) \
       dbapi$(STATIC) dbapi_util_blobstore$(STATIC) dbapi_driver$(STATIC) \
       $(XCONNEXT) xconnect $(COMPRESS_LIBS) xutil test_boost xncbi

LIBS = $(SYBASE_LIBS) $(SYBASE_DLLS) $(ODBC_LIBS) $(FTDS64_LIBS) \
       $(FTDS95_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_REQUIRES = in-house-resources

CHECK_COPY = dbapi_unit_test.ini

CHECK_TIMEOUT = 600

# CHECK_CMD = dbapi_unit_test -dr dblib      -S MsSql
CHECK_CMD = dbapi_unit_test -dr ftds64     -S MsSql
CHECK_CMD = dbapi_unit_test -dr ftds95     -S MsSql
CHECK_CMD = dbapi_unit_test -dr ftds95     -S MsSql -V 73
CHECK_CMD = dbapi_unit_test -dr odbc       -S MsSql
# CHECK_CMD = dbapi_unit_test -dr dblib      -S DBAPI_MS2014_TEST
CHECK_CMD = dbapi_unit_test -dr ftds64     -S DBAPI_MS2014_TEST
CHECK_CMD = dbapi_unit_test -dr ftds95     -S DBAPI_MS2014_TEST
CHECK_CMD = dbapi_unit_test -dr ftds95     -S DBAPI_MS2014_TEST -V 73
CHECK_CMD = dbapi_unit_test -dr odbc       -S DBAPI_MS2014_TEST
# Force the traditional C locale when using Sybase ctlib to avoid
# error #4847 from Sybase ASE 15.5 (reporting that character set
# mismatches block bulk insertion).
CHECK_CMD = env LC_ALL=C dbapi_unit_test -dr ctlib -S Sybase
CHECK_CMD = dbapi_unit_test -dr dblib      -S Sybase
CHECK_CMD = dbapi_unit_test -dr ftds64     -S Sybase
CHECK_CMD = dbapi_unit_test -dr ftds95     -S Sybase

WATCHERS = ucko
