# $Id$

APP = convert_seq
SRC = convert_seq
LIB = xformat xalnmgr gbseq xobjutil xobjread creaders tables submit \
      $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects dbapi -Cygwin
