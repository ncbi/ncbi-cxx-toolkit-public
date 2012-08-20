# $Id$

APP = sdbapi_unit_test
SRC = sdbapi_unit_test sdbapi_unit_test_object sdbapi_unit_test_bcp \
      sdbapi_unit_test_proc sdbapi_unit_test_stmt \
      sdbapi_unit_test_connection sdbapi_unit_test_common \
      sdbapi_unit_test_lob

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = test_boost $(SDBAPI_LIB) xconnect xutil xncbi

LIBS = $(SDBAPI_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_COPY = sdbapi_unit_test.ini

CHECK_TIMEOUT = 600

CHECK_CMD = sdbapi_unit_test -S MsSql
CHECK_CMD = sdbapi_unit_test -S Sybase
# Run tests against Sybase ASE 15.5 (below) as well as Sybase 12.x (above)
CHECK_CMD = sdbapi_unit_test -S DBAPI_SYB155_TEST -T Sybase

WATCHERS = ucko
