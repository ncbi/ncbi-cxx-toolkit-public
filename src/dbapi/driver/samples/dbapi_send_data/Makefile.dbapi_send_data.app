# $Id$

APP = dbapi_send_data
SRC = dbapi_send_data dbapi_send_data_util

LIB  = dbapi_driver xncbi
LIBS = $(NETWORK_LIBS) $(NCBI_C_LIBPATH) $(ORIG_LIBS) $(DL_LIBS)