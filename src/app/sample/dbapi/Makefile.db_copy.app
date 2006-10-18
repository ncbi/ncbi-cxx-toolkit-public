# $Id$

REQUIRES = dbapi

APP = db_copy
SRC = db_copy

LIB  = dbapi dbapi_driver $(XCONNEXT) xconnect xncbi
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

