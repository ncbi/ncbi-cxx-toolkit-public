#################################
# $Id$
#################################

REQUIRES = dbapi

APP = test_objmgr_gbloader
SRC = test_objmgr_gbloader
LIB = $(BDB_CACHE_LIB) $(BDB_LIB) $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(BERKELEYDB_LIBS) $(ORIG_LIBS) 

CHECK_CMD = test_objmgr_loaders.sh test_objmgr_gbloader
CHECK_COPY = test_objmgr_loaders.sh
