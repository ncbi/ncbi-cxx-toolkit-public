# $Id$

APP = prime_cache
SRC = prime_cache

CPPFLAGS = $(SRA_INCLUDE) $(ORIG_CPPFLAGS)

LIB  = asn_cache \
           bdb $(OBJREAD_LIBS) taxon1 $(SRAREAD_LIBS) \
	   xobjutil $(OBJMGR_LIBS)

LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(FTDS_LIBS) \
	   $(SRA_SDK_SYSLIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = $(VDB_REQ)
WATCHERS = marksc2
