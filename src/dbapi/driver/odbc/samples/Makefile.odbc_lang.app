# $Id$

APP = odbc_lang
SRC = odbc_lang

REQUIRES = ODBC

LIB  = ncbi_xdbapi_odbc dbapi_driver $(XCONNEXT) xconnect xncbi
LIBS = $(ODBC_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ODBC_INCLUDE) $(ORIG_CPPFLAGS)

CHECK_REQUIRES = in-house-resources
CHECK_COPY = odbc_lang.ini
CHECK_CMD =

WATCHERS = ucko
