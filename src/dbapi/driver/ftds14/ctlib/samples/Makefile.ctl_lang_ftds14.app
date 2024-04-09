# $Id$

APP = ctl_lang_ftds14
SRC = ctl_lang_ftds14 dbapi_driver_sample_base_ftds14

LIB  = ncbi_xdbapi_ftds14 $(FTDS14_CTLIB_LIB) \
       dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(FTDS14_CTLIB_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = -DFTDS_IN_USE -I$(includedir)/dbapi/driver/ftds14 \
           $(FTDS14_INCLUDE) $(ORIG_CPPFLAGS)

CHECK_REQUIRES = in-house-resources
# CHECK_CMD = run_sybase_app.sh ctl_lang_ftds14
CHECK_CMD = run_sybase_app.sh ctl_lang_ftds14 -S DBAPI_MS2022_TEST_LB /CHECK_NAME=ctl_lang_ftds14-MS2022
CHECK_CMD = run_sybase_app.sh ctl_lang_ftds14 -S DBAPI_DEV16_2K -v 50 /CHECK_NAME=ctl_lang_ftds14-SYB16-2K
CHECK_CMD = run_sybase_app.sh ctl_lang_ftds14 -S DBAPI_DEV16_16K -v 50 /CHECK_NAME=ctl_lang_ftds14-SYB16-16K

WATCHERS = ucko satskyse
