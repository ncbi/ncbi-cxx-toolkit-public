# $Id$

APP = dbapi_cache_test
SRC = dbapi_cache_test

REQUIRES = ODBC


LIB  = dbapi_cache dbapi_driver xutil dbapi xncbi dbapi_driver_odbc 

LIBS = $(ODBC_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)
