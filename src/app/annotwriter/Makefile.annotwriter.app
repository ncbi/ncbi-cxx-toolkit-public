#################################
# $Id$
# Author:  Mati Shomrat
#################################

# Build application "annotwriter"
#################################

APP = annotwriter
SRC = annotwriter
LIB = xalgoseq xobjwrite $(XFORMAT_LIBS) variation_utils $(OBJEDIT_LIBS) xalnmgr \
      xobjutil entrez2cli entrez2 tables xregexp $(PCRE_LIB) $(DATA_LOADERS_UTIL_LIB) $(OBJMGR_LIBS)

LIBS = $(DATA_LOADERS_UTIL_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = BerkeleyDB objects -Cygwin

WATCHERS = ludwigf

POST_LINK = $(VDB_POST_LINK)
