#################################
# $Id$
#################################

REQUIRES = dbapi

APP = seqvec_bench
SRC = seqvec_bench
LIB = test_mt $(NOBJMGR_LIBS)

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

