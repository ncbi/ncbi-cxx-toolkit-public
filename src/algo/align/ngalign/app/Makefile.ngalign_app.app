APP = ngalign_app
SRC = ngalign_app

ASN_DEP = seq 

LIB_ = xngalign xmergetree ncbi_xloader_asn_cache asn_cache \
      bdb xalgoalignnw xalgoalignutil xalgoseq blastinput \
      $(BLAST_DB_DATA_LOADER_LIBS) align_format $(BLAST_LIBS) \
      gene_info taxon1 xcgi xhtml xregexp $(PCRE_LIB) xqueryparse \
      $(GENBANK_LIBS)

LIB = $(LIB_:%=%$(STATIC)) $(FTDS_LIB)

LIBS = $(BERKELEYDB_LIBS) $(PCRE_LIBS) $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)

REQUIRES = objects

WATCHERS = boukn

