# $Id$

REQUIRES = objects dbapi

APP = convert_seq
SRC = convert_seq
LIB = xformat xalnmgr gbseq xobjutil xobjread tables submit $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
