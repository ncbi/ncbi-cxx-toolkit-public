# $Id$

ASN_DEP = seq seqalign

APP = demo_contig_assembly
SRC = demo_contig_assembly
LIB = xalgocontig_assembly xblast seqdb blastdb composition_adjustment \
      xobjsimple xnetblastcli xconnect xalgodustmask xnetblast scoremat \
      seqset xalgoalignnw $(SEQ_LIBS) pub medline biblio general xser \
      xutil xalgoseq xregexp xncbi $(PCRE_LIB) xobjutil xalnmgr tables \
      $(OBJMGR_LIBS) 


CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)
LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects

