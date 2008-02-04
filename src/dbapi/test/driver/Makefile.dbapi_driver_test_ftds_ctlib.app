# $Id$

APP = dbapi_driver_test_ftds_ctlib
SRC = dbapi_driver_test_ftds_ctlib

LIB  = dbapi$(STATIC) dbapi_driver$(STATIC) xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS)

CHECK_CMD = run_sybase_app.sh dbapi_driver_test_ftds_ctlib

CHECK_REQUIRES = Sybase DLL

