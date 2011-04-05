#################################
# $Id$
#################################

WATCHERS = kiryutin kapustin

APP = splign
SRC = splign_app

LIB = xalgoalignsplign xalgoalignutil xalgoalignnw \
      $(BLAST_DB_DATA_LOADER_LIBS) \
      ncbi_xloader_lds lds bdb\
      xalgoseq $(PCRE_LIB) \
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
