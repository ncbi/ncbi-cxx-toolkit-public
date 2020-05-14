# $Id$
APP = unit_test_entrezgene
SRC = unit_test_entrezgene

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = entrezgene $(SEQ_LIBS) pub medline biblio general $(EUTILS_LIBS) xconnect xser xutil test_boost xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = unit_test_entrezgene

WATCHERS = wallinc
