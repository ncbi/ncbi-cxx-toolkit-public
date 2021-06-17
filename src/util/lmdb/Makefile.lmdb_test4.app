# $Id$

SRC = mtest4
APP = lmdb_test4
PROJ_TAG = test

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(includedir)/util/lmdb

LIB = $(LMDB_LIB)
LIBS = $(LMDB_LIBS)

CHECK_COPY = lmdb_test.sh
CHECK_CMD  = lmdb_test.sh 4

REQUIRES = -Cygwin

WATCHERS = ivanov
