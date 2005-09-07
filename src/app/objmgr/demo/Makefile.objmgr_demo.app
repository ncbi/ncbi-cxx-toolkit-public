#################################
# $Id$
# Author:  Aleksey Grichenko (grichenk@ncbi.nlm.nih.gov)
#################################

# Build object manager demo application "objmgr_demo"
#################################

REQUIRES = objects

APP = objmgr_demo
SRC = objmgr_demo
BLAST = ncbi_xloader_blastdb seqdb xnetblastcli xnetblast scoremat blastdb tables
LIB = $(BLAST) ncbi_xloader_lds lds_admin lds bdb xobjread xobjutil $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(BERKELEYDB_LIBS) $(ORIG_LIBS)
