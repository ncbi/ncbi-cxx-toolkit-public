#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "serialtest"
#################################

REQUIRES = dbapi

APP = reader_pubseq_test
SRC = reader_pubseq_test

LIB = id1 seqset $(SEQ_LIBS) pub medline biblio general \
      dbapi_driver \
      xser xconnect xutil xncbi 

PRE_LIBS =  -L.. -lxobjmgr1

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

