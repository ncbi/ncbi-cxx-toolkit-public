# $Id$

APP = autodef_demo
SRC = autodef_demo

LIB = xobjutil xconnect xregexp $(PCRE_LIB) $(SOBJMGR_LIBS) $(OBJEDIT_LIBS)

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = bollin
