# $Id$

APP = aln_rng_coll
SRC = aln_rng_coll_app ../aln_rng_coll ../diag_rng_coll
LIB = submit $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
