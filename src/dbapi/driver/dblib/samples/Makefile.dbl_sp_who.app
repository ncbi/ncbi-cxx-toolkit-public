# $Id$

APP = dbl_sp_who
SRC = dbl_sp_who

LIB  = ncbi_xdbapi_dblib dbapi_driver xncbi
LIBS = $(SYBASE_DBLIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(SYBASE_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = Sybase DBLib

CHECK_CMD = run_sybase_app.sh dbl_sp_who
