# $Id$

APP = bdbtest
SRC = test_bdb
LIB = $(BDB_CACHE_LIB) $(BDB_LIB) $(CMPRS_LIB) xcompress xutil xncbi 
LIBS = $(BERKELEYDB_LIBS) $(DL_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE)

WATCHERS = satskyse
