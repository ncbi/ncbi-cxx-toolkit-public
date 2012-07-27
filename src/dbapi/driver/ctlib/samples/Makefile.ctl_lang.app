# $Id$

APP = ctl_lang
SRC = ctl_lang dbapi_driver_sample_base_ctl

LIB  = ncbi_xdbapi_ctlib$(STATIC) dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(SYBASE_LIBS) $(SYBASE_DLLS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = -I$(includedir)/dbapi/driver/ctlib $(SYBASE_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = Sybase

CHECK_REQUIRES = in-house-resources
CHECK_COPY = ctl_lang.ini
CHECK_CMD = run_sybase_app.sh ctl_lang /CHECK_NAME=ctl_lang

WATCHERS = ucko
