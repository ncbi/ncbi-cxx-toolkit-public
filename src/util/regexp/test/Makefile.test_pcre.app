# $Id$

SRC = pcretest
APP = test_pcre

CPPFLAGS = $(ORIG_CPPFLAGS) -I$(includedir)/util/regexp -I$(srcdir)/..

LIB = regexp

CHECK_CMD = test_pcre.sh
CHECK_COPY = testdata test_pcre.sh
