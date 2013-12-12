# $Id$

APP = autodef_demo
SRC = autodef_demo

LIB = xregexp $(PCRE_LIB) xobjedit taxon3 xconnect xobjutil $(SOBJMGR_LIBS)

LIBS = $(DL_LIBS) $(ORIG_LIBS) $(NETWORK_LIBS) $(PCRE_LIBS)

WATCHERS = bollin
