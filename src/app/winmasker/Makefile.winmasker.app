# $Id$

REQUIRES = objects algo

ASN_DEP = seq

APP = windowmasker
SRC = main win_mask_app win_mask_config win_mask_dup_table \
      win_mask_gen_counts win_mask_util win_mask_sdust_masker

LIB = xalgowinmask xalgodustmask blast composition_adjustment seqdb blastdb \
	seqmasks_io tables xobjread creaders xobjutil \
	$(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

