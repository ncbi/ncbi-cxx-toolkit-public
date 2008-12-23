# $Id$

APP = agp_validate
SRC = agp_validate ContextValidator AgpErrEx AltValidator
# AccessionPatterns LineValidator - dropped
# AgpErr - renamed to AgpErrEx

LIB = entrez2cli entrez2 taxon1 xobjutil xobjread xidmapper \
      $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin

