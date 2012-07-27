# $Id$

APP = dbl_sp_who
SRC = dbl_sp_who

LIB  = ncbi_xdbapi_dblib$(STATIC) dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(SYBASE_DBLIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = $(SYBASE_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = Sybase DBLib

CHECK_REQUIRES = in-house-resources
CHECK_COPY = dbl_sp_who.ini
CHECK_CMD = run_sybase_app.sh dbl_sp_who /CHECK_NAME=dbl_sp_who

WATCHERS = ucko
