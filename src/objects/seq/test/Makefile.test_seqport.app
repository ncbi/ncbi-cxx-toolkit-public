# $Id$

APP = test_seqport
OBJ = test_seqport

LIB = seq seqfeat seqloc seqcode seqalign seqblock seqset pub biblio pubmed \
      medline seqres general xser xutil xncbi

LIBS     = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
