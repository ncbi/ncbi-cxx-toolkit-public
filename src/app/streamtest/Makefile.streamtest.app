#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "streamtest"
#################################

APP = streamtest
SRC = streamtest
LIB = prosplign xalgoalignutil xalgoseq xmlwrapp xcleanup xobjwrite xformat xalnmgr \
      xobjutil taxon1 gbseq submit gbseq xqueryparse xregexp tables \
      $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) \
      $(PCRE_LIBS) $(ORIG_LIBS)

REQUIRES = objects algo LIBXSLT

WATCHERS = kans
