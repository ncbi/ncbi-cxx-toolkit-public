# $Id$

APP = dbapi_send_data
SRC = dbapi_send_data

LIB  = dbapi_sample_base dbapi_driver xconnext xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)
