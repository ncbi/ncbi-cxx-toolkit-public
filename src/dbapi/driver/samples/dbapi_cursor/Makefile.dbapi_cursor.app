# $Id$

APP = dbapi_cursor
SRC = dbapi_cursor

LIB  = dbapi_sample_base$(STATIC) $(DBAPI_CTLIB) $(DBAPI_ODBC) \
       ncbi_xdbapi_ftds ncbi_xdbapi_ftds64 $(FTDS64_LIB) \
       ncbi_xdbapi_ftds95 $(FTDS95_LIB) dbapi_driver$(STATIC) \
       $(XCONNEXT) xconnect xutil xncbi

LIBS = $(SYBASE_LIBS) $(SYBASE_DLLS) $(ODBC_LIBS) $(FTDS64_LIBS) \
       $(FTDS95_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_COPY = dbapi_cursor.ini

WATCHERS = ucko
