# $Id$

APP = agp_validate
SRC = agp_validate AltValidator AgpFastaComparator

LIB  = $(DATA_LOADERS_UTIL_LIB) \
	   ncbi_xloader_lds2 lds2 $(OBJREAD_LIBS) \
	   xobjutil taxon1 entrez2cli \
       entrez2 sqlitewrapp $(OBJMGR_LIBS)

LIBS = $(DATA_LOADERS_UTIL_LIBS) \
	   $(GENBANK_THIRD_PARTY_LIBS) $(SQLITE3_STATIC_LIBS) \
	   $(CMPRS_LIBS) $(NETWORK_LIBS) \
       $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin SQLITE3
CXXFLAGS = $(ORIG_CXXFLAGS)
# -Wno-parentheses

WATCHERS = sapojnik drozdov foleyjp
POST_LINK = $(VDB_POST_LINK)

