#################################
# $Id$
# Author:  Aleksey Grichenko (grichenk@ncbi.nlm.nih.gov)
#################################

# Build object manager test application "testobjmgr_mt"
#################################

REQUIRES = dbapi MT

APP = testobjmgr1_mt
SRC = testobjmgr1_mt test_helper
LIB = xobjutil xobjmgr id1 submit seqset $(SEQ_LIBS) pub medline biblio \
      general dbapi_driver xser xutil xconnect test_mt xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = testobjmgr1_mt

