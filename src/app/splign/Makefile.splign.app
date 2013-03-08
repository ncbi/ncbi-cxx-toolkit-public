#################################
# $Id$
#################################

WATCHERS = kiryutin kapustin

APP = splign
SRC = splign_app

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
