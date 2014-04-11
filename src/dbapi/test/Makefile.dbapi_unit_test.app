# $Id$

APP = dbapi_unit_test
SRC = dbapi_unit_test dbapi_unit_test_object dbapi_unit_test_lob dbapi_unit_test_bcp \
      dbapi_unit_test_proc dbapi_unit_test_cursor dbapi_unit_test_stmt \
      dbapi_unit_test_connection dbapi_unit_test_timeout dbapi_unit_test_context \
      dbapi_unit_test_msg dbapi_unit_test_common

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = dbapi$(STATIC) dbapi_driver$(STATIC) dbapi_util_blobstore$(STATIC) \
       $(COMPRESS_LIBS) $(XCONNEXT) test_boost$(STATIC) xconnect xutil xncbi
STATIC_LIB = $(DBAPI_CTLIB) $(DBAPI_ODBC) ncbi_xdbapi_ftds $(FTDS_LIB) $(LIB)

LIBS =  $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
STATIC_LIBS = $(SYBASE_LIBS) $(SYBASE_DLLS) $(ODBC_LIBS) \
              $(Z_LIBS) $(FTDS_LIBS) $(LIBS)

REQUIRES = Boost.Test.Included

CHECK_REQUIRES = in-house-resources

CHECK_COPY = dbapi_unit_test.ini

CHECK_TIMEOUT = 600

# CHECK_CMD = dbapi_unit_test -dr dblib      -S MsSql
CHECK_CMD = dbapi_unit_test -dr ftds       -S MsSql
CHECK_CMD = dbapi_unit_test -dr odbc       -S MsSql
# Force the traditional C locale when using Sybase ctlib to avoid
# error #4847 from Sybase ASE 15.5 (reporting that character set
# mismatches block bulk insertion).
CHECK_CMD = env LC_ALL=C dbapi_unit_test -dr ctlib -S Sybase
CHECK_CMD = dbapi_unit_test -dr dblib      -S Sybase
CHECK_CMD = dbapi_unit_test -dr ftds       -S Sybase

WATCHERS = ucko
