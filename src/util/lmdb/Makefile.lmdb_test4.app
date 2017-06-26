# $Id$

REQUIRES = LocalLMDB

SRC = mtest4
APP = lmdb_test4
PROJ_TAG = test

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(includedir)/util/lmdb

LIB = $(LMDB_LIB)
LIBS = $(LMDB_LIBS)

CHECK_CMD =
CHECK_COPY = testdb

WATCHERS = ivanov
