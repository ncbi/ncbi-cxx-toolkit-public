#################################
# $Id$
#################################

REQUIRES = dbapi

APP = test_objmgr1_gbloader
SRC = test_objmgr1_gbloader
LIB = xobjutil xobjmgr id1 submit seqset $(SEQ_LIBS) pub medline biblio \
      general dbapi_driver xser xutil xconnect xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

#CHECK_CMD = test_objmgr1_gbloader
