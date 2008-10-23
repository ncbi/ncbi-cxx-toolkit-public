#################################
# $Id$
#################################


APP = test_objmgr_data_mt
SRC = test_objmgr_data_mt
LIB = test_mt ncbi_xloader_blastdb seqdb blastdb ncbi_xloader_lds lds bdb xobjread xobjutil $(OBJMGR_LIBS) $(DBAPI_CTLIB:%=%$(STATIC)) dbapi_driver$(STATIC)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(BERKELEYDB_LIBS) $(SYBASE_LIBS) $(ORIG_LIBS)

CHECK_COPY = test_objmgr_loaders_id1.sh test_objmgr_loaders_id2.sh test_objmgr_loaders_pubseqos.sh test_objmgr_data_mt.sh test_objmgr_data_ids.sh test_objmgr_data.id1 test_objmgr_data.id2
CHECK_CMD = test_objmgr_loaders_id2.sh test_objmgr_data_ids.sh test_objmgr_data_mt /CHECK_NAME=test_objmgr_data_mt
CHECK_CMD = test_objmgr_loaders_id1.sh test_objmgr_data_mt.sh /CHECK_NAME=test_objmgr_data_mt.sh+id1
CHECK_CMD = test_objmgr_loaders_id2.sh test_objmgr_data_mt.sh /CHECK_NAME=test_objmgr_data_mt.sh+id2
CHECK_CMD = test_objmgr_loaders_pubseqos.sh test_objmgr_data_mt.sh /CHECK_NAME=test_objmgr_data_mt.sh+pubseqos
CHECK_TIMEOUT = 1800
