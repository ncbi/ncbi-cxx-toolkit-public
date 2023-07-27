############################
# $Id$
############################

APP = compartp
SRC = compartp

LIB = prosplign xalgoalignutil xalgoseq taxon1 $(BLAST_LIBS) xqueryparse \
      xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(BLAST_THIRD_PARTY_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) \
       $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

WATCHERS = chetvern
