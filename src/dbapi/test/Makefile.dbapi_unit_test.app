# $Id$

APP = dbapi_unit_test
SRC = dbapi_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = dbapi dbapi_driver xncbi
LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_REQUIRES = DLL
CHECK_COPY = dbapi_unit_test.sh
CHECK_CMD = dbapi_unit_test.sh
