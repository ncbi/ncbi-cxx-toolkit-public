#################################
# $Id$
#################################

REQUIRES = dbapi

APP = test_objmgr_data
SRC = test_objmgr_data
LIB = $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_objmgr_loaders_id1.sh test_objmgr_data.sh
CHECK_CMD = test_objmgr_loaders_id2.sh test_objmgr_data.sh
CHECK_CMD = test_objmgr_loaders_pubseqos.sh test_objmgr_data.sh
CHECK_COPY = test_objmgr_loaders_id1.sh test_objmgr_loaders_id2.sh test_objmgr_loaders_pubseqos.sh test_objmgr_data.sh
