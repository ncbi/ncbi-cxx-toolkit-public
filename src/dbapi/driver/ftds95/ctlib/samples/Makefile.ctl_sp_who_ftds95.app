# $Id$

APP = ctl_sp_who_ftds95
SRC = ctl_sp_who_ftds95 dbapi_driver_sample_base_ftds95

LIB  = ncbi_xdbapi_ftds95$(STATIC) $(FTDS95_CTLIB_LIB) \
       dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(FTDS95_CTLIB_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = -DFTDS_IN_USE -I$(includedir)/dbapi/driver/ftds95 \
           $(FTDS95_INCLUDE) $(ORIG_CPPFLAGS)

CHECK_REQUIRES = in-house-resources
# CHECK_CMD = run_sybase_app.sh ctl_sp_who_ftds95
CHECK_CMD = run_sybase_app.sh ctl_sp_who_ftds95 -S MSDEV1 /CHECK_NAME=ctl_sp_who_ftds95-MS
CHECK_CMD = run_sybase_app.sh ctl_sp_who_ftds95 -S MSDEV4 /CHECK_NAME=ctl_sp_who_ftds95-MS2014

WATCHERS = ucko
