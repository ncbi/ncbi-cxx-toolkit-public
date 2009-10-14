# $Id$

REQUIRES = LocalPCRE

SRC = pcretest
APP = test_pcre

CPPFLAGS = -I$(includedir)/util/regexp -I$(srcdir)/.. -DHAVE_CONFIG_H $(ORIG_CPPFLAGS)

LIB = $(PCRE_LIB)
LIBS = $(PCRE_LIBS)

CHECK_CMD = test_pcre.sh
CHECK_COPY = testdata test_pcre.sh

WATCHERS = ivanov
