# $Id$

APP = bdb_cverify
SRC = bdb_cverify
LIB = bdb_cache bdb xncbi xutil
LIBS = $(BERKELEYDB_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE)
