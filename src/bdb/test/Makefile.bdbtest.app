# $Id$

APP = bdbtest
SRC = test_bdb
LIB = bdb xncbi xutil
LIBS = $(BERKELEYDB_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE)
