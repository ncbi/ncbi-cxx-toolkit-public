#################################
# $Id$
# Author:  Aleksey Grichenko (grichenk@ncbi.nlm.nih.gov)
#################################

# Build object manager demo application "objmgr_demo"
#################################

REQUIRES = dbapi

APP = objmgr_demo
SRC = objmgr_demo
LIB = bdb $(OBJMGR_LIBS)

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(BERKELEYDB_LIBS) $(ORIG_LIBS)
