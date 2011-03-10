#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "streamtest"
#################################

APP = streamtest
SRC = streamtest
LIB = xformat xobjutil submit gbseq xalnmgr xcleanup xregexp prosplign xalgoalignutil \
      xqueryparse xobjwrite tables $(COMPRESS_LIBS) $(OBJMGR_LIBS) $(PCRE_LIB)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(PCRE_LIBS) $(ORIG_LIBS)

REQUIRES = objects algo

WATCHERS = kans
