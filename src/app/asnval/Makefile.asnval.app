#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "asnval"
#################################

APP = asnvalidate
SRC = asnval
LIB = xvalidate $(OBJEDIT_LIBS) $(XFORMAT_LIBS) xalnmgr xobjutil \
	  data_loaders_util \
	  ncbi_xloader_asn_cache asn_cache \
	  ncbi_xloader_lds2 lds2 sqlitewrapp \
	  bdb \
	  $(BLAST_DB_DATA_LOADER_LIBS) $(BLAST_LIBS) \
	  $(ncbi_xreader_pubseqos2) \
	  submit \
	  ncbi_xloader_csra ncbi_xloader_wgs \
	  $(SRAREAD_LIBS) \
	  ncbi_xdbapi_ftds dbapi dbapi_driver $(FTDS_LIB) \
	  xmlwrapp valerr \
	  tables xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

#LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(LIBXML_LIBS)
LIBS = $(VDB_LIBS) $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(PCRE_LIBS) \
       $(FTDS_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)


REQUIRES = objects LIBXML LIBXSLT $(VDB_REQ)

CXXFLAGS += $(ORIG_CXXFLAGS)
LDFLAGS  += $(ORIG_LDFLAGS)

WATCHERS = bollin gotvyans
