#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "agp_fasta_compare"
#################################

APP = agp_fasta_compare
SRC = agp_fasta_compare

LIB  = ncbi_xloader_lds2 lds2 xobjread xobjutil taxon1 entrez2cli entrez2 submit sqlitewrapp creaders $(OBJMGR_LIBS)
LIBS = $(SQLITE3_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin SQLITE3


WATCHERS = bollin kornbluh
