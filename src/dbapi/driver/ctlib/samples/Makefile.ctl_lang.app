# $Id$

APP = ctl_lang
SRC = ctl_lang

LIB  = ncbi_xdbapi_ctlib$(STATIC) dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(SYBASE_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = $(SYBASE_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = Sybase

CHECK_COPY = ctl_lang.ini
CHECK_CMD = run_sybase_app.sh ctl_lang
