# $Id$

APP = dbapi_cache_admin
SRC = dbapi_cache_admin


LIB  = ncbi_xcache_dbapi dbapi dbapi_driver_ftds $(FTDS8_LIB) dbapi_driver \
       xutil xncbi

LIBS = $(FTDS8_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
