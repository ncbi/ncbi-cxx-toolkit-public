#################################
# $Id$
#################################

APP = test_pubmed_article
SRC = test_pubmed_article
LIB = efetch pubmed medline biblio general xser xutil xncbi

LIBS = $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = grichenk
