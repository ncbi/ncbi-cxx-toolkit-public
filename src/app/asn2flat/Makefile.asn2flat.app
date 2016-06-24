#################################
# $Id$
# Author:  Mati Shomrat
#################################

# Build application "asn2flat"
#################################

APP = asn2flat
SRC = asn2flat

LIB  = $(OBJREAD_LIBS) $(XFORMAT_LIBS) valerr \
       $(ncbi_xloader_wgs) $(SRAREAD_LIBS) \
           xalnmgr xobjutil entrez2cli entrez2 tables \
	   ncbi_xdbapi_ftds dbapi $(ncbi_xreader_pubseqos2) $(FTDS_LIB) \
	   xregexp $(PCRE_LIB) $(OBJMGR_LIBS)
LIBS = $(VDB_LIBS) $(FTDS_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) \
	   $(DL_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects

WATCHERS = ludwigf gotvyans
