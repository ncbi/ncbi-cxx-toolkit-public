# $Id$

APP = ctl_sp_who
OBJ = ctl_sp_who

LIB        = dbapi_driver_samples dbapi_driver_ctlib dbapi_driver xncbi
LIBS       = $(SYBASE_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(SYBASE_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = Sybase
