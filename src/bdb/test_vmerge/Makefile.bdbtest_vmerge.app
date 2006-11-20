# $Id$

APP = bdbtest_vmerge
SRC = test_bdb_vmerge
LIB = $(BDB_CACHE_LIB) $(BDB_LIB) xalgovmerge xncbi xutil
LIBS = $(BERKELEYDB_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE)
