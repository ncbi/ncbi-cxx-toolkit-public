# $Id$

APP = ctl_lang
SRC = ctl_lang

LIB  = ncbi_xdbapi_ctlib dbapi_driver xncbi
LIBS = $(SYBASE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(SYBASE_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = Sybase

CHECK_CMD = run_sybase_app.sh ctl_lang
