#

WATCHERS = astashya

SRC = xcompareannotsdemo
APP = xcompareannotsdemo

CPPFLAGS = $(ORIG_CPPFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIB = xalgoseq xalnmgr tables xregexp $(PCRE_LIB) xobjutil taxon1 $(OBJMGR_LIBS) ncbi_xloader_lds lds bdb xobjread creaders

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS) $(BERKELEYDB_LIBS)

REQUIRES = bdb
