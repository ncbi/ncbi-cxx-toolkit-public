#################################
# $Id$
# Author:  Michael Kornbluh
#################################

# Build application "gap_stats"
#################################

APP = gap_stats
SRC = gap_stats

LIB  = xmlwrapp xalgoseq $(OBJREAD_LIBS) taxon1 xalnmgr xobjutil tables xregexp $(PCRE_LIB) $(OBJMGR_LIBS)
LIBS = $(LIBXSLT_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) $(LIBXML_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


REQUIRES = objects LIBXML LIBXSLT algo

WATCHERS = kornbluh

