# $Id$

REQUIRES = LocalPCRE

SRC = pcre2test
APP = test_pcre
PROJ_TAG = test

CPPFLAGS = -I$(includedir)/util/regexp -I$(srcdir)/.. -DHAVE_CONFIG_H \
           $(ORIG_CPPFLAGS)

LIB = $(PCRE_LIB)
LIBS = $(PCRE_LIBS)

# automatic test is disabled, test manually
#CHECK_CMD = test_pcre.sh
#CHECK_COPY = testdata test_pcre.sh

WATCHERS = ivanov ucko
