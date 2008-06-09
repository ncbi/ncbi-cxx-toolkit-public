#

APP = xcompareannotsdemo
SRC = xcompareannotsdemo

CPPFLAGS = $(ORIG_CPPFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIB = xalgoseq taxon1 xalnmgr ncbi_xloader_lds lds bdb xobjread creaders \
      xobjutil tables xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) \
       $(DL_LIBS) $(ORIG_LIBS)
