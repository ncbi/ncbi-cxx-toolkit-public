# $Id$

REQUIRES = dbapi

APP = alnvwr
SRC = alnvwr
LIB = xalnmgr submit tables $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
