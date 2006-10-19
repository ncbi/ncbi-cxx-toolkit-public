# $Id$

APP = pairwise_aln
SRC = pairwise_aln_app ../pairwise_aln ../diag_rng_coll
LIB = submit $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
