# $Id$

WATCHERS = jcherry 

ASN_DEP = seq

APP = demo_seqtest
SRC = demo_seqtest
LIB = xalgoseqqa xalgognomon xalgoseq xalnmgr xobjutil seqtest entrez2cli entrez2 \
        tables xregexp taxon1 $(PCRE_LIB) $(OBJMGR_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)
LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = -Cygwin
