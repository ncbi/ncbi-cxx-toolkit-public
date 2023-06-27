# $Id$

APP = pubmed_citmatch
SRC = pubmed_citmatch
LIB = $(OBJEDIT_LIBS) mlacli mla eutils uilist efetch seqset $(SEQ_LIBS) \
      pubmed medlars pub medline biblio general xser xconnect xutil xncbi

LIBS = $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

WATCHERS = stakhovv
