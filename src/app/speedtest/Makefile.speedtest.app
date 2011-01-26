#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "speedtest"
#################################

APP = speedtest
SRC = speedtest
LIB = prosplign xalgoalignutil xqueryparse xalnmgr xcleanup xregexp xobjutil \
      submit tables $(OBJMGR_LIBS:%=%$(STATIC)) $(PCRE_LIBS)

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(PCRE_LIBS) $(ORIG_LIBS)

REQUIRES = objects algo -Cygwin


WATCHERS = ludwigf
