# $Id$

APP = aln_build
SRC = aln_build_app ../sparse_aln ../sparse_ci
LIB = submit $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
