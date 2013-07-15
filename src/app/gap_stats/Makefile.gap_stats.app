#################################
# $Id$
# Author:  Michael Kornbluh
#################################

# Build application "gap_stats"
#################################

APP = gap_stats
SRC = gap_stats

LIB  = xalgoseq $(OBJREAD_LIBS) taxon1 xalnmgr xobjutil tables \
    xregexp $(PCRE_LIB) $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects algo

WATCHERS = kornbluh

