#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "serialtest"
#################################

APP = reader_id1_test
SRC = reader_id1_test
LIB = xobjmgr1 seqset $(SEQ_LIBS) pub medline biblio general xser xutil xncbi id1 xconnect

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)

