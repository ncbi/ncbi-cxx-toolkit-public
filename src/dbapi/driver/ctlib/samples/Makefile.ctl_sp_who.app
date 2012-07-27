# $Id$

APP = ctl_sp_who
SRC = ctl_sp_who dbapi_driver_sample_base_ctl

LIB  = ncbi_xdbapi_ctlib$(STATIC) dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(SYBASE_LIBS) $(SYBASE_DLLS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = -I$(includedir)/dbapi/driver/ctlib  $(SYBASE_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = Sybase

CHECK_REQUIRES = in-house-resources
CHECK_COPY = ctl_sp_who.ini
CHECK_CMD = run_sybase_app.sh ctl_sp_who /CHECK_NAME=ctl_sp_who

WATCHERS = ucko
