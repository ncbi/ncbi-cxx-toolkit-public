# $Id$

APP = dbapi_driver_test_ctlib
SRC = dbapi_driver_test_ctlib

LIB  = ncbi_xdbapi_ctlib$(STATIC) dbapi_driver$(STATIC) test_boost$(STATIC) xncbi
PRE_LIBS = $(BOOST_LIBS)
LIBS = $(SYBASE_LIBS) $(SYBASE_DLLS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = $(SYBASE_INCLUDE) $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

# CHECK_CMD = run_sybase_app.sh dbapi_driver_test_ctlib

REQUIRES = Sybase Boost.Test

