#################################
# $Id$
# Author:  Aaron Ucko (ucko@ncbi.nlm.nih.gov)
#################################

# Build relative-location-computation test application "test_relloc"
#################################

REQUIRES = dbapi

APP = test_relloc
SRC = test_relloc
LIB = xobjutil xobjmgr id1 submit seqset $(SEQ_LIBS) pub medline biblio \
      general dbapi_driver xser xutil xconnect xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
