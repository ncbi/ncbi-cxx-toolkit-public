# $Id$

SRC = pcretest
APP = test_regexp

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(includedir)/util/regexp -I$(srcdir)/..

LIB = regexp

CHECK_CMD = test_regexp.sh
CHECK_COPY = testdata test_regexp.sh
