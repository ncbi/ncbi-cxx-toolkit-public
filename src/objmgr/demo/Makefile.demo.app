#################################
# $Id$
# Author:  Aleksey Grichenko (grichenk@ncbi.nlm.nih.gov)
#################################

# Build object manager demo application "objmgr_demo"
#################################

REQUIRES = bdb

APP = objmgr_demo
SRC = objmgr_demo
LIB = bdb $(GENBANK_LIBS) $(GENBANK_READER_ID1C_LIBS)

LIBS += $(BERKELEYDB_LIBS)
