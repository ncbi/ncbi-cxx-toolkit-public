#################################
# $Id$
# Author:  Aaron Ucko (ucko@ncbi.nlm.nih.gov)
#################################

# Build title-computation test application "test_title"
#################################

REQUIRES = dbapi

APP = test_objmgr_title
SRC = test_objmgr_title
LIB = $(NOBJMGR_LIBS)

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

