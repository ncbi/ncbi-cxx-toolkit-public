# $Id$

APP = bdbtest
SRC = test_bdb
LIB = bdb_cache bdb xncbi xutil
LIBS = $(BERKELEYDB_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE)
