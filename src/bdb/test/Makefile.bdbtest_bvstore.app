# $Id$

APP = bdbtest_bvstore
SRC = test_bdb_bvstore
LIB =  $(BDB_LIB) xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = MT bdb


CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE)
