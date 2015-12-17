# $Id$

APP = ctl_sp_databases_ftds64
SRC = ctl_sp_databases_ftds64 dbapi_driver_sample_base_driver

LIB  = dbapi$(STATIC) ncbi_xdbapi_$(ftds64)$(STATIC) $(FTDS64_CTLIB_LIB) dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(FTDS64_CTLIB_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = -DFTDS_IN_USE $(FTDS64_INCLUDE) $(ORIG_CPPFLAGS)

CHECK_REQUIRES = in-house-resources
# CHECK_CMD = run_sybase_app.sh ctl_sp_databases_ftds64
CHECK_CMD = run_sybase_app.sh ctl_sp_databases_ftds64 -S MSDEV1 /CHECK_NAME=ctl_sp_databases_ftds64-MS

REQUIRES = FreeTDS

WATCHERS = ucko
