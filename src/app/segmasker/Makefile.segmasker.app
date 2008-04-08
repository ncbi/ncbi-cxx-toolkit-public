# $Id$

REQUIRES = objects algo

ASN_DEP = seq

APP = segmasker
SRC = segmasker seg

LIB_ = xobjsimple seqmasks_io xalgowinmask $(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

