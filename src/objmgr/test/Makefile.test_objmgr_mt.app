#################################
# $Id$
# Author:  Aleksey Grichenko (grichenk@ncbi.nlm.nih.gov)
#################################

# Build object manager test application "testobjmgr_mt"
#################################

REQUIRES = dbapi

APP = testobjmgr1_mt
SRC = testobjmgr1_mt test_helper
LIB = xobjmgr1 id1 seqset $(SEQ_LIBS) pub medline biblio general \
      dbapi_driver xser xutil xconnect xncbi test_mt

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = testobjmgr1_mt

REQUIRES = MT

