#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "serialtest"
#################################

REQUIRES = dbapi

APP = reader_pubseq_test
SRC = reader_pubseq_test

LIB = xobjmgr id1 seqset $(SEQ_LIBS) pub medline biblio general \
      dbapi_driver \
      xser xconnect xutil xncbi 

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

