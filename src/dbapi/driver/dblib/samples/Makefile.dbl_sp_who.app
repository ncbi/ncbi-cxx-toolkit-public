# $Id$

APP = dbl_sp_who
OBJ = dbl_sp_who

LIB        = dbapi_driver_samples dbapi_driver_dblib dbapi_driver xncbi
LIBS       = $(SYBASE_DBLIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(SYBASE_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = Sybase DBLib
