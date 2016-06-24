#################################
# $Id$
# Author:  Mati Shomrat
#################################

# Build application "asn2fasta"
#################################

APP = asn2fasta
SRC = asn2fasta
LIB = $(ncbi_xloader_wgs) $(SRAREAD_LIBS) \
          xobjwrite xobjread $(XFORMAT_LIBS) xalnmgr xobjutil valerr xregexp \
	  ncbi_xdbapi_ftds dbapi $(ncbi_xreader_pubseqos2) $(FTDS_LIB) \
	  entrez2cli entrez2 tables $(OBJMGR_LIBS) $(PCRE_LIB)

LIBS = $(FTDS_LIBS) $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) \
	   $(DL_LIBS) $(PCRE_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects


WATCHERS = ludwigf foleyjp
