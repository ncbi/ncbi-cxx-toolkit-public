# $Id$

APP = dbapi_driver_test_ftds_ctlib
SRC = dbapi_driver_test_ftds_ctlib

LIB  = dbapi$(STATIC) ncbi_xdbapi_ftds$(STATIC) $(FTDS_LIB) ncbi_xdbapi_ctlib$(STATIC) dbapi_driver$(STATIC) xncbi
LIBS = $(FTDS_LIBS) $(SYBASE_LIBS) $(SYBASE_DLLS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)
# The line below causes this test to crash ...
# LIBS = $(SYBASE_DBLIBS) $(SYBASE_LIBS) $(SYBASE_DLLS) $(FTDS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS)

CHECK_REQUIRES = in-house-resources
CHECK_CMD = run_sybase_app.sh dbapi_driver_test_ftds_ctlib /CHECK_NAME=dbapi_driver_test_ftds_ctlib

REQUIRES = FreeTDS Sybase

WATCHERS = ucko
