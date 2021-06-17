# $Id$

SRC = mtest3
APP = lmdb_test3
PROJ_TAG = test

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(includedir)/util/lmdb

LIB = $(LMDB_LIB)
LIBS = $(LMDB_LIBS)

CHECK_COPY = lmdb_test.sh
CHECK_CMD  = lmdb_test.sh 3

REQUIRES = -Cygwin

WATCHERS = ivanov
