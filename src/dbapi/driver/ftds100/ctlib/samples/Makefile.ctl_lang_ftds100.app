# $Id$

APP = ctl_lang_ftds100
SRC = ctl_lang_ftds100 dbapi_driver_sample_base_ftds100

LIB  = ncbi_xdbapi_ftds100$(STATIC) $(FTDS100_CTLIB_LIB) \
       dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(FTDS100_CTLIB_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = -DFTDS_IN_USE -I$(includedir)/dbapi/driver/ftds100 \
           $(FTDS100_INCLUDE) $(ORIG_CPPFLAGS)

CHECK_REQUIRES = connext in-house-resources
# CHECK_CMD = run_sybase_app.sh ctl_lang_ftds100
CHECK_CMD = run_sybase_app.sh ctl_lang_ftds100 -S DBAPI_MS2017_TEST_LB /CHECK_NAME=ctl_lang_ftds100-MS2017
CHECK_CMD = run_sybase_app.sh ctl_lang_ftds100 -S DBAPI_SYB160_TEST -v 50 /CHECK_NAME=ctl_lang_ftds100-SYB16-2K
CHECK_CMD = run_sybase_app.sh ctl_lang_ftds100 -S DBAPI_DEV16_16K -v 50 /CHECK_NAME=ctl_lang_ftds100-SYB16-16K

WATCHERS = ucko satskyse
