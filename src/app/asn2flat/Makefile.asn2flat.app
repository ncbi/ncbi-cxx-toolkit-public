#################################
# $Id$
# Author:  Mati Shomrat
#################################

# Build application "asn2flat"
#################################

APP = asn2flat
SRC = asn2flat

CDD_LIB = ncbi_xloader_cdd cdd_access
SNP_LIB = $(ncbi_xloader_snp) dbsnp_ptis grpc_integration

LIB  = $(OBJREAD_LIBS) $(XFORMAT_LIBS) valerr\
       $(ncbi_xloader_wgs) $(SRAREAD_LIBS)\
       $(SNP_LIB) $(CDD_LIB)\
       xalnmgr xobjutil entrez2cli entrez2 tables\
       ncbi_xdbapi_ftds dbapi $(ncbi_xreader_pubseqos2) $(FTDS_LIB)\
       xregexp $(PCRE_LIB)  $(SRAREAD_LIBS) $(DATA_LOADERS_UTIL_LIB) $(OBJMGR_LIBS)
LIBS = $(DATA_LOADERS_UTIL_LIBS) $(VDB_LIBS) $(FTDS_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS)\
	   $(DL_LIBS) $(ORIG_LIBS)\
       $(GRPC_LIBS) $(GENBANK_THIRD_PARTY_STATIC_LIBS)	   

CPPFLAGS += $(GRPC_INCLUDE)

POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects BerkeleyDB SQLITE3

WATCHERS = ludwigf gotvyans dondosha
