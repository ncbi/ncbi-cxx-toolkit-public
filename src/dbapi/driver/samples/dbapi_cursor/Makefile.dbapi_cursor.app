# $Id$

APP = dbapi_cursor
SRC = dbapi_cursor dbapi_cursor_util

LIB  = dbapi_driver xncbi
LIBS = $(NETWORK_LIBS) $(NCBI_C_LIBPATH) $(ORIG_LIBS) $(DL_LIBS)
