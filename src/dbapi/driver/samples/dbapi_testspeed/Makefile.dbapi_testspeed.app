# $Id$

APP = dbapi_testspeed
SRC = dbapi_testspeed

LIB  = dbapi_sample_base dbapi_driver $(XCONNEXT) xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CHECK_COPY = dbapi_testspeed.ini
