#################################
# $Id$
#################################

REQUIRES = dbapi FreeTDS

APP = test_objmgr_gbloader_mt
SRC = test_objmgr_gbloader_mt
LIB = test_mt $(OBJMGR_LIBS) ncbi_xdbapi_ftds $(FTDS_LIB) dbapi_driver$(STATIC)

LIBS = $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = all_readers.sh test_objmgr_gbloader_mt /CHECK_NAME=test_objmgr_gbloader_mt
CHECK_COPY = all_readers.sh
CHECK_TIMEOUT = 400

WATCHERS = vasilche
