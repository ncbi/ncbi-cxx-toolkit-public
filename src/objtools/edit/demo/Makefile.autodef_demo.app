# $Id$

APP = autodef_demo
SRC = autodef_demo

LIB = xobjedit xobjutil valid taxon3 xconnect xregexp $(PCRE_LIB) $(SOBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = bollin
