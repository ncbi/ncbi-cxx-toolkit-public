# $Id$

APP = dbapi_cache_admin
SRC = dbapi_cache_admin


LIB  = dbapi_cache dbapi_driver xutil dbapi xncbi \
       dbapi_driver_ftds 

LIBS = $(ODBC_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)
