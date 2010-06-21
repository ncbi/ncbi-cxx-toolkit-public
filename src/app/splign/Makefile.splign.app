#################################
# $Id$
#################################

APP = splign
SRC = splign_app


BLAST_DB_DATA_LOADER_LIBS = ncbi_xloader_blastdb ncbi_xloader_blastdb_rmt
LIB = xalgoalignsplign xalgoalignutil xqueryparse xalgoalignnw xalgoseq \
      $(BLAST_DB_DATA_LOADER_LIBS) \
	  ncbi_xloader_lds lds bdb \
      xalnmgr xregexp $(PCRE_LIB) taxon1 \
      $(BLAST_LIBS:%=%$(STATIC)) \
      $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(BERKELEYDB_STATIC_LIBS) \
       $(PCRE_LIBS) \
       $(CMPRS_LIBS) \
       $(NETWORK_LIBS) \
       $(DL_LIBS) \
       $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)

LDFLAGS  = $(FAST_LDFLAGS)

REQUIRES = algo BerkeleyDB objects -Cygwin
