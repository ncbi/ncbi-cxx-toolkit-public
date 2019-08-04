# $Id$

APP = merge_tree
SRC = merge_tree_app  


LIB = xmergetree xalgoalignutil xalnmgr tables scoremat \
      $(GENBANK_LIBS) $(SEQ_LIBS) 
LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(BLAST_THIRD_PARTY_LIBS) $(ORIG_LIBS)
