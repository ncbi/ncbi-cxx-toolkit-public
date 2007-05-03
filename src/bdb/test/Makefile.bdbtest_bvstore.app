# $Id$

APP = bdbtest_bvstore
SRC = test_bdb_bvstore
LIB =  $(BDB_LIB) $(COMPRESS_LIBS) xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = MT bdb


CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE)
