# $Id$

APP = ftds_sp_who
SRC = ftds_sp_who

LIB  = dbapi_driver_ftds $(FTDS8_LIB) dbapi_driver xncbi
LIBS = $(FTDS8_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(FTDS8_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = FreeTDS

CHECK_CMD =
