# $Id$

APP = bdb_dump
SRC = bdb_dumper
LIB = bdb xncbi xutil
LIBS = $(BERKELEYDB_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BERKELEYDB_INCLUDE)
