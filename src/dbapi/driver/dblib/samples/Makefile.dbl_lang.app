# $Id$

APP = dbl_lang
OBJ = dbl_lang

LIB        = dbapi_driver_samples dbapi_driver_dblib dbapi_driver xncbi
LIBS       = $(SYBASE_DBLIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(SYBASE_INCLUDE) $(ORIG_CPPFLAGS)
