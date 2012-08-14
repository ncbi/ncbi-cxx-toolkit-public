# $Id$

APP = convert_seq
SRC = convert_seq
LIB = xformat xalnmgr gbseq xobjutil $(OBJREAD_LIBS) tables submit \
      xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin

WATCHERS = ucko
