# $Id$

WATCHERS = jcherry

ASN_DEP = seq

APP = demo_contig_assembly
SRC = demo_contig_assembly
LIB = xalgocontig_assembly xalgoalignnw xalgoseq xregexp $(PCRE_LIB) \
      $(BLAST_LIBS) taxon1 $(OBJMGR_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)
LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects

