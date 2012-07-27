# $Id$

APP = odbc_sp_who
SRC = odbc_sp_who

REQUIRES = ODBC

LIB  = ncbi_xdbapi_odbc dbapi_driver $(XCONNEXT) xconnect xncbi
LIBS = $(ODBC_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ODBC_INCLUDE) $(ORIG_CPPFLAGS)

CHECK_REQUIRES = in-house-resources
CHECK_COPY = odbc_sp_who.ini
CHECK_CMD =

WATCHERS = ucko
