# $Id$

APP = dbapi_bcp
SRC = dbapi_bcp

LIB  = dbapi_sample_base$(STATIC) $(DBAPI_CTLIB) $(DBAPI_ODBC) \
       ncbi_xdbapi_ftds ncbi_xdbapi_ftds100 $(FTDS100_LIB) \
       ncbi_xdbapi_ftds14 $(FTDS14_LIB) \
       dbapi_driver$(DLL) $(XCONNEXT) xconnect xutil xncbi

LIBS = $(SYBASE_LIBS) $(SYBASE_DLLS) $(ODBC_LIBS) $(FTDS100_LIBS) \
       $(FTDS14_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_COPY = dbapi_bcp.ini

WATCHERS = ucko satskyse
