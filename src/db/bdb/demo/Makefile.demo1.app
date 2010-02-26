# $Id$

APP = bdb_demo1
SRC = demo1
LIB = bdb xutil xncbi
LIBS = $(BERKELEYDB_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE)

WATCHERS = kuznets
