# $Id$

REQUIRES = dbapi

APP = alnmrg
SRC = alnmrg
LIB = xalnmgr submit $(OBJMGR_LIBS)

LIBS = $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
