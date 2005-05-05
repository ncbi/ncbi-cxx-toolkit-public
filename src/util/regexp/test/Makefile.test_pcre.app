# $Id$

SRC = pcretest
APP = test_pcre

CPPFLAGS = -I$(includedir)/util/regexp -I$(srcdir)/.. $(ORIG_CPPFLAGS)

LIB = regexp

CHECK_CMD = test_pcre.sh
CHECK_COPY = testdata test_pcre.sh
