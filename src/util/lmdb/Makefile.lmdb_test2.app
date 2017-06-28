# $Id$

SRC = mtest2
APP = lmdb_test2
PROJ_TAG = test

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(includedir)/util/lmdb

LIB = $(LMDB_LIB)
LIBS = $(LMDB_LIBS)

WATCHERS = ivanov
