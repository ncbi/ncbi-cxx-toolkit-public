# $Id$

APP = bdbtest_vmerge
SRC = test_bdb_vmerge
LIB = xalgovmerge $(BDB_CACHE_LIB) $(BDB_LIB) $(COMPRESS_LIBS) xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE)
