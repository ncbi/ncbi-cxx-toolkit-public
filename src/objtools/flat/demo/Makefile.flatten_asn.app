# $Id$

# Build app "flatten_asn"
###############################

APP = flatten_asn
SRC = flatten_asn
LIB = xflat xobjutil xalnmgr xobjmgr id1 gbseq seqset $(SEQ_LIBS) \
      pub medline biblio general dbapi_driver xser xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
