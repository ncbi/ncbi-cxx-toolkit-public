# $Id$

APP = ftds_sp_who
SRC = ftds_sp_who

LIB  = dbapi_driver_ftds dbapi_driver xncbi
LIBS = $(FTDS8_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
DLL_LIB = $(FTDS8_LIB)

CPPFLAGS = $(FTDS8_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = FreeTDS

CHECK_CMD =
