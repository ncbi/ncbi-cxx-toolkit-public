# $Id$

APP = odbc_lang
SRC = odbc_lang

LIB  = dbapi_driver_odbc dbapi_driver xncbi
LIBS = $(ODBC_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ODBC_INCLUDE) $(ORIG_CPPFLAGS)

CHECK_CMD =
