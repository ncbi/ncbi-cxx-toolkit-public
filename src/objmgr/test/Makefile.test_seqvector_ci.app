#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build object manager test application "test_objmgr"
#################################

REQUIRES = dbapi

APP = test_seqvector_ci
SRC = test_seqvector_ci
LIB = xobjmgr id1 seqset $(SEQ_LIBS) pub medline biblio \
      general dbapi_driver xser xutil xconnect xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_seqvector_ci
CHECK_TIMEOUT = 250
# LINK = quantify $(ORIG_LINK)
