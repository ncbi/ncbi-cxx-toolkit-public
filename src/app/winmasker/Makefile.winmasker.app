# $Id$

ASN_DEP = seq

APP = winmasker
SRC = main win_mask_app win_mask_config win_mask_dup_table \
	  win_mask_fasta_reader win_mask_gen_counts win_mask_reader \
	  win_mask_seq_title win_mask_writer win_mask_writer_fasta \
	  win_mask_writer_int

LIB = xalgowinmask \
	  xblast xnetblastcli xnetblast scoremat seqdb blastdb tables \
	  xobjread xobjutil $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

