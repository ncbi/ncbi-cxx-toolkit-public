#################################
# $Id$
#################################

REQUIRES = dbapi

APP = test_objmgr_data_mt
SRC = test_objmgr_data_mt
LIB = test_mt $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = test_objmgr_loaders_id1.sh test_objmgr_loaders_id2.sh test_objmgr_loaders_pubseqos.sh test_objmgr_data_mt.sh test_objmgr_data_ids.sh test_objmgr_data.id1 test_objmgr_data.id2
CHECK_CMD = test_objmgr_loaders_id2.sh test_objmgr_data_ids.sh test_objmgr_data_mt
CHECK_CMD = test_objmgr_loaders_id1.sh test_objmgr_data_mt.sh
CHECK_CMD = test_objmgr_loaders_id2.sh test_objmgr_data_mt.sh
CHECK_CMD = test_objmgr_loaders_pubseqos.sh test_objmgr_data_mt.sh
CHECK_TIMEOUT = 1000
