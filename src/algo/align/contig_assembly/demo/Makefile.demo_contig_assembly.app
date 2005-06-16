# $Id$

ASN_DEP = seq seqalign

APP = demo_contig_assembly
SRC = demo_contig_assembly
LIB = xalgocontig_assembly xblast xnetblast scoremat \
   xalgoalignnw xalgoseq xobjutil \
	xalnmgr tables $(OBJMGR_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects

