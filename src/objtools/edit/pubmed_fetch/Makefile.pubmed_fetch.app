# $Id$

APP = pubmed_fetch
SRC = pubmed_fetch
LIB = xobjedit mlacli mla eutils uilist efetch pubmed medlars pub medline biblio general xser xconnect xutil xncbi

LIBS = $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

WATCHERS = stakhovv
