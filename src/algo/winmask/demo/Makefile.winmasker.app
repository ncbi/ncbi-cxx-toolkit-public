# $Id$

ASN_DEP = seq

APP = winmasker
SRC = main win_mask_app
LIB = xalgowinmask \
	  xblast xnetblastcli xnetblast scoremat seqdb blastdb tables \
	  xobjread xobjutil $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

