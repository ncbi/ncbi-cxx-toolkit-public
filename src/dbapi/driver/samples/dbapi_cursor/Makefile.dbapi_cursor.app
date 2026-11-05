# $Id$

APP = dbapi_cursor
SRC = dbapi_cursor

LIB  = dbapi_sample_base$(STATIC) $(DBAPI_CTLIB) $(DBAPI_ODBC) \
       ncbi_xdbapi_ftds ncbi_xdbapi_ftds14 $(FTDS14_LIB) \
       dbapi_driver$(DLL) $(XCONNEXT) xconnect xutil xncbi

LIBS = $(SYBASE_LIBS) $(SYBASE_DLLS) $(ODBC_LIBS) \
       $(FTDS14_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_COPY = dbapi_cursor.ini

WATCHERS = ucko satskyse
