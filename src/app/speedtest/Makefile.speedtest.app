#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "speedtest"
#################################

APP = speedtest
SRC = speedtest
LIB = prosplign xalgoalignutil taxon1 xalgoseq xcleanup xobjedit taxon3 valid valerr \
      $(BLAST_LIBS) xqueryparse xregexp $(PCRE_LIB) $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(BLAST_THIRD_PARTY_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) \
       $(PCRE_LIBS) $(ORIG_LIBS)

REQUIRES = objects algo -Cygwin


WATCHERS = gotvyans
