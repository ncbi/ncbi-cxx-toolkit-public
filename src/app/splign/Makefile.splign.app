#################################
# $Id$
#################################

WATCHERS = kiryutin kapustin

APP = splign
SRC = splign_app


#################################
# LDS version:

# REQUIRES = algo BerkeleyDB objects -Cygwin

# LIB = xalgoalignsplign xalgoalignutil xalgoalignnw \
#       $(BLAST_DB_DATA_LOADER_LIBS) \
#       ncbi_xloader_lds lds bdb\
#       xqueryparse xalgoseq $(PCRE_LIB) \
#       $(BLAST_LIBS:%=%$(STATIC)) \
#       $(OBJMGR_LIBS:%=%$(STATIC))

# LIBS = $(BERKELEYDB_STATIC_LIBS) \
#        $(PCRE_LIBS) \
#        $(CMPRS_LIBS) \
#        $(NETWORK_LIBS) \
#        $(DL_LIBS) \
#        $(ORIG_LIBS)


#################################
# LDS2 version:

REQUIRES = algo SQLITE3 objects -Cygwin

LIB = xalgoalignsplign xalgoalignutil xalgoalignnw \
      $(BLAST_DB_DATA_LOADER_LIBS) \
      ncbi_xloader_lds2 lds2 sqlitewrapp \
      xqueryparse xalgoseq $(PCRE_LIB) \
      $(BLAST_LIBS:%=%$(STATIC)) submit \
      $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(PCRE_LIBS) \
       $(CMPRS_LIBS) \
       $(NETWORK_LIBS) \
       $(SQLITE3_LIBS) \
       $(DL_LIBS) \
       $(ORIG_LIBS)


CXXFLAGS = $(FAST_CXXFLAGS)

LDFLAGS  = $(FAST_LDFLAGS)

