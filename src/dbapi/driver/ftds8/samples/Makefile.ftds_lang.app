# $Id$

APP = ftds_lang
SRC = ftds_lang

LIB  = dbapi_driver_ftds dbapi_driver xncbi
LIBS = $(FTDS8_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
DLL_LIB = $(FTDS8_LIB)

CPPFLAGS = $(FTDS8_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = FreeTDS

CHECK_CMD =
