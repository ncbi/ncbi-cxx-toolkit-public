# $Id$

APP = dbapi_cache_test
SRC = dbapi_cache_test

REQUIRES = ODBC


LIB  = ncbi_xcache_dbapi dbapi_driver xutil dbapi xncbi ncbi_xdbapi_odbc

LIBS = $(ODBC_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)
