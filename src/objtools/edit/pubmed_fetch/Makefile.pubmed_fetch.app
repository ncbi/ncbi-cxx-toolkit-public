# $Id$

APP = pubmed_fetch
SRC = pubmed_fetch
LIB = mlacli mla eutils uilist efetch pubmed medline biblio general xser xutil xncbi

LIBS = $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = stakhovv
