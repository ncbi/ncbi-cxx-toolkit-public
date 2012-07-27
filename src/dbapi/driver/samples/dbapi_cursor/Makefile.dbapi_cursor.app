# $Id$

APP = dbapi_cursor
SRC = dbapi_cursor

LIB  = dbapi_sample_base$(STATIC) dbapi_driver$(STATIC) $(XCONNEXT) xconnect xutil xncbi
STATIC_LIB = dbapi_sample_base$(STATIC) $(DBAPI_CTLIB) $(DBAPI_ODBC) ncbi_xdbapi_ftds $(FTDS_LIB) $(LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)
STATIC_LIBS = $(SYBASE_LIBS) $(ODBC_LIBS) $(FTDS_LIBS) $(LIBS)

CHECK_COPY = dbapi_cursor.ini

WATCHERS = ucko
