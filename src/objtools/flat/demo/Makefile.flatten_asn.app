# $Id$

# Build app "flatten_asn"
###############################

APP = flatten_asn
SRC = flatten_asn
LIB = xflat xobjutil xalnmgr tables gbseq $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = dbapi
