# $Id$

APP = prime_cache
SRC = prime_cache

CPPFLAGS = $(SRA_INCLUDE) $(SQLITE3_INCLUDE) $(ORIG_CPPFLAGS)

LIB  = asn_cache \
           bdb $(OBJREAD_LIBS) local_taxon taxon1 $(SRAREAD_LIBS) \
	   xobjutil $(OBJMGR_LIBS) sqlitewrapp

LIBS = $(SQLITE3_LIBS) $(GENBANK_THIRD_PARTY_LIBS) $(BERKELEYDB_LIBS) \
       $(CMPRS_LIBS) $(FTDS_LIBS) $(SRA_SDK_SYSLIBS) $(DL_LIBS) \
       $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = SQLITE3

WATCHERS = marksc2
