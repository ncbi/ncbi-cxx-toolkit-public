#################################
# $Id$
#################################

REQUIRES = dbapi MT

APP = test_objmgr_gbloader_mt
SRC = test_objmgr_gbloader_mt
LIB = xobjmgr id1 seqset $(SEQ_LIBS) pub medline biblio general \
  dbapi_driver xser xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

# CHECK_CMD = test_objmgr_loaders.sh test_objmgr_gbloader_mt
# CHECK_COPY = test_objmgr_loaders.sh
