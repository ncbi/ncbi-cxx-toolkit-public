# $Id$

APP = sub_cache_create
SRC = sub_cache_create

LIB  = $(DATA_LOADERS_UTIL_LIB) $(SRAREAD_LIBS) xobjutil taxon1 $(SDBAPI_LIB) \
       $(OBJMGR_LIBS)
LIBS = $(DATA_LOADERS_UTIL_LIBS) $(SDBAPI_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) \
       $(ORIG_LIBS)

REQUIRES = BerkeleyDB SQLITE3

WATCHERS = mozese2
