# $Id$

APP = dbapi_query
SRC = dbapi_query

LIB  = dbapi_sample_base dbapi_driver $(XCONNEXT) xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)


