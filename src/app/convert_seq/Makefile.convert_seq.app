# $Id$

REQUIRES = objects dbapi

APP = convert_seq
SRC = convert_seq
LIB = xflat xalnmgr gbseq xobjutil xobjread tables $(OBJMGR_LIBS)

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
