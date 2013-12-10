# $Id$

APP = seqdb_demo
SRC = seqdb_demo
LIB_ = seqdb xobjutil blastdb $(SOBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

CFLAGS    = $(FAST_CFLAGS)
CXXFLAGS  = $(FAST_CXXFLAGS)
LDFLAGS   = $(FAST_LDFLAGS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = madden camacho
