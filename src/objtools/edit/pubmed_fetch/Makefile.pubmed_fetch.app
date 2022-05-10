# $Id$

APP = pubmed_fetch
SRC = pubmed_fetch
LIB = $(OBJEDIT_LIBS) mlacli mla eutils uilist efetch $(SEQ_LIBS) \
      pubmed medlars pub medline biblio general xser xconnect xutil xncbi

LIBS = $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

WATCHERS = stakhovv
