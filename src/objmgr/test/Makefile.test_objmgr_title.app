#################################
# $Id$
# Author:  Aaron Ucko (ucko@ncbi.nlm.nih.gov)
#################################

# Build title-computation test application "test_title"
#################################

REQUIRES = dbapi

APP = test_title
SRC = test_title
LIB = xobjmgr1 id1 seqset $(SEQ_LIBS) pub medline biblio general \
      dbapi_driver xser xutil xncbi xconnect

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)

