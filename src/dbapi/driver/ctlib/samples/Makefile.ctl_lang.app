# $Id$

APP = ctl_lang
SRC = ctl_lang

LIB        = dbapi_driver_samples dbapi_driver_ctlib dbapi_driver xncbi
LIBS       = $(SYBASE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(SYBASE_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = Sybase
