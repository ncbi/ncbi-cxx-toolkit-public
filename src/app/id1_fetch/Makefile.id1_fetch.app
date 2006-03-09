# $Id$

APP = id1_fetch
SRC = id1_fetch
LIB = xformat xalnmgr gbseq xobjutil id1cli submit entrez2cli entrez2 tables \
      $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin
