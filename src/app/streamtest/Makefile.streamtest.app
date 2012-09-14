#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "streamtest"
#################################

APP = streamtest
SRC = streamtest
LIB = prosplign xalgoalignutil xalgoseq xmlwrapp xvalidate xcleanup xobjwrite $(XFORMAT_LIBS) xalnmgr \
      xobjutil valid valerr taxon1 gbseq submit taxon3 gbseq xqueryparse xregexp tables \
      $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) \
      $(PCRE_LIBS) $(ORIG_LIBS)

REQUIRES = objects algo LIBXSLT

WATCHERS = kans
