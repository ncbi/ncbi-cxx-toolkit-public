#################################
# $Id$
# Author:  Aleksey Grichenko (grichenk@ncbi.nlm.nih.gov)
#################################

# Build object manager demo application "objmgr_demo"
#################################

REQUIRES = objects BerkeleyDB

APP = objmgr_demo
SRC = objmgr_demo
LIB = $(BDB_LIB) $(OBJMGR_LIBS) $(GENBANK_READER_ID1C_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(BERKELEYDB_LIBS) $(ORIG_LIBS)
