#################################
# $Id$
#################################

REQUIRES = dbapi

APP = test_objmgr_data_mt
SRC = test_objmgr_data_mt
LIB = xobjmgr id1 seqset $(SEQ_LIBS) pub medline biblio general dbapi_driver xser xconnect xutil xncbi test_mt

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_objmgr_loaders.sh test_objmgr_data_mt
CHECK_COPY = test_objmgr_loaders.sh
CHECK_TIMEOUT = 1000
