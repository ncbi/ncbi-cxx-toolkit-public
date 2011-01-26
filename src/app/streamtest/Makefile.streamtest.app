#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "streamtest"
#################################

APP = streamtest
SRC = streamtest
LIB = xobjutil xcleanup xregexp prosplign submit xalgoalignutil xqueryparse \
      xalnmgr tables $(COMPRESS_LIBS) $(SOBJMGR_LIBS) $(PCRE_LIB)

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(PCRE_LIBS) $(ORIG_LIBS)

REQUIRES = objects algo

WATCHERS = kans
