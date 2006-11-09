# $Id$

APP = aln_build
SRC = aln_build_app ../pairwise_aln ../diag_rng_coll
LIB = submit $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
