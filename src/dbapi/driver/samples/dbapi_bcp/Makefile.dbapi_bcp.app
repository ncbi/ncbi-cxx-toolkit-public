# $Id$

APP = dbapi_bcp
SRC = dbapi_bcp dbapi_bcp_util

LIB  = dbapi_driver xncbi
LIBS = $(NETWORK_LIBS) $(NCBI_C_LIBPATH) $(ORIG_LIBS) $(DL_LIBS)
