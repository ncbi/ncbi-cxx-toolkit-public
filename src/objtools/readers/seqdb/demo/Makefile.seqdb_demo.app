# $Id$

APP = seqdb_demo
SRC = seqdb_demo
LIB = seqdb xobjutil blastdb $(OBJMGR_LIBS)

CFLAGS    = $(FAST_CFLAGS)
CXXFLAGS  = $(FAST_CXXFLAGS)
LDFLAGS   = $(FAST_LDFLAGS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = dbapi
