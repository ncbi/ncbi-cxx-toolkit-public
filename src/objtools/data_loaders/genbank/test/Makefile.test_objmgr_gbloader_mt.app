#################################
# $Id$
#################################


APP = test_objmgr_gbloader_mt
SRC = test_objmgr_gbloader_mt
LIB = test_mt $(OBJMGR_LIBS) $(DBAPI_CTLIB:%=%$(STATIC)) dbapi_driver$(STATIC)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(SYBASE_LIBS) $(ORIG_LIBS)

CHECK_CMD = run_sybase_app.sh test_objmgr_loaders.sh test_objmgr_gbloader_mt /CHECK_NAME=test_objmgr_gbloader_mt
CHECK_COPY = test_objmgr_loaders.sh
CHECK_TIMEOUT = 400
