#################################
# $Id$
#################################

REQUIRES = bdb dbapi FreeTDS

APP = test_objmgr_gbloader
SRC = test_objmgr_gbloader
LIB = $(BDB_CACHE_LIB) $(BDB_LIB) ncbi_xdbapi_ftds $(OBJMGR_LIBS) $(FTDS_LIB)

LIBS = $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(BERKELEYDB_LIBS) $(ORIG_LIBS)

CHECK_CMD = all_readers.sh test_objmgr_gbloader /CHECK_NAME=test_objmgr_gbloader
CHECK_COPY = all_readers.sh

WATCHERS = vasilche
