# $Id$

APP = id1_fetch
SRC = id1_fetch
LIB = $(XFORMAT_LIBS) xalnmgr gbseq xobjutil id1cli submit entrez2cli entrez2 tables \
      xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin

WATCHERS = ucko
