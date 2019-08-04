# $Id$
#################################
# Build demo "compart"

APP = compart
SRC = compart

LIB =  xalgoalignsplign xalgoalignutil xalgoalignnw xalgoseq taxon1 \
       $(BLAST_LIBS:%=%$(STATIC)) xqueryparse xregexp $(PCRE_LIB) \
       $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(BLAST_THIRD_PARTY_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

REQUIRES = -Cygwin

WATCHERS = chetvern
