# $Id$

APP = python_ncbi_dbapi_test
SRC = python_ncbi_dbapi_test

REQUIRES = PYTHON Boost.Test

CPPFLAGS = $(ORIG_CPPFLAGS) $(PYTHON_INCLUDE) $(BOOST_INCLUDE)

LIB  = xutil xncbi
LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(PYTHON_LIBS) $(ORIG_LIBS)

# CHECK_REQUIRES = mswin
CHECK_REQUIRES = DLL
CHECK_CMD = python_ncbi_dbapi_test.sh
CHECK_COPY = python_ncbi_dbapi_test.sh
