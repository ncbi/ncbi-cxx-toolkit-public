# $Id$

APP = agp_validate
SRC = agp_validate ContextValidator AgpErrEx AltValidator
# AccessionPatterns LineValidator - dropped
# AgpErr - renamed to AgpErrEx

LIB = entrez2cli entrez2 taxon1 xobjutil $(OBJMGR_LIBS:%=%$(STATIC))\
  xobjread seqset $(SEQ_LIBS) pub medline biblio general xser xutil xncbi

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin

