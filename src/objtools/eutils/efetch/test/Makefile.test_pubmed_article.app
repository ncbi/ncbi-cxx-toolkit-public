#################################
# $Id$
#################################

APP = test_pubmed_article
SRC = test_pubmed_article
LIB = efetch pubmed medline biblio general xser xutil xncbi

LIBS = $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = data

CHECK_CMD = test_pubmed_article -f data/pubmed11748933.xml -set
CHECK_CMD = test_pubmed_article -f data/pubmed11748934.xml
CHECK_CMD = test_pubmed_article -f data/pubmed28211659.xml -set
CHECK_CMD = test_pubmed_article -f data/pubmed31732993.xml -set
CHECK_CMD = test_pubmed_article -f data/pubmed33761533.xml -set

WATCHERS = grichenk
