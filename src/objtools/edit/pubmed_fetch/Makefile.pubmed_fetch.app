# $Id$

APP = pubmed_fetch
SRC = pubmed_fetch
LIB = $(OBJEDIT_LIBS) eutils uilist pub xser xconnect xutil xncbi

LIBS = $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

WATCHERS = stakhovv
