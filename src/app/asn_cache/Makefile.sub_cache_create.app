# $Id$

APP = sub_cache_create
SRC = sub_cache_create

LIB = data_loaders_util ncbi_xloader_asn_cache asn_cache \
	ncbi_xloader_lds2 lds2 sqlitewrapp \
	$(BLAST_DB_DATA_LOADER_LIBS) $(BLAST_LIBS) \
	$(ncbi_xreader_pubseqos2) \
	$(SDBAPI_LIB) \
	submit \
	ncbi_xloader_csra \
	ncbi_xloader_wgs  $(SRAREAD_LIBS) ncbi_xdbapi_mysql \
	bdb $(OBJMGR_LIBS)
LIBS = $(SQLITE3_LIBS) $(SRA_SDK_SYSLIBS) $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(GNUTLS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(FTDS_LIBS)

WATCHERS = marksc2
