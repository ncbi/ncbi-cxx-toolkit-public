# $Id$

APP = agp_validate
SRC = agp_validate AltValidator AgpFastaComparator

LIB  = ncbi_xloader_lds2 lds2 xobjread xobjutil taxon1 entrez2cli \
       entrez2 submit sqlitewrapp creaders $(OBJMGR_LIBS)
LIBS = $(SQLITE3_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) \
       $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin
CXXFLAGS = $(ORIG_CXXFLAGS)
# -Wno-parentheses

WATCHERS = sapojnik
