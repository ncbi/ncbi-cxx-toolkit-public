# $Id$

REQUIRES = LocalPCRE

SRC = pcretest pcre_printint
APP = test_pcre
PROJ_TAG = test

CPPFLAGS = -I$(includedir)/util/regexp -I$(srcdir)/.. -DHAVE_CONFIG_H -DSUPPORT_PCRE8 $(ORIG_CPPFLAGS)

LIB = $(PCRE_LIB)
LIBS = $(PCRE_LIBS)

# automatic test is disabled, test manually
#CHECK_CMD = test_pcre.sh
#CHECK_COPY = testdata test_pcre.sh

WATCHERS = ivanov
