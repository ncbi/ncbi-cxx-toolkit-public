# $Id$

APP = odbc_sp_who
SRC = odbc_sp_who

LIB  = ncbi_xdbapi_odbc dbapi_driver xncbi
LIBS = $(ODBC_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ODBC_INCLUDE) $(ORIG_CPPFLAGS)

CHECK_CMD =
