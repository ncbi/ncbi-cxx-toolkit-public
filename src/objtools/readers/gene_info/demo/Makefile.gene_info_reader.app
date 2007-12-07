# $Id$

REQUIRES = dbapi objects algo

ASN_DEP = seq

APP = gene_info_reader
SRC = gene_info_reader_app

LIB = gene_info xobjutil xobjsimple seqdb \
	$(BLAST_LIBS) $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

