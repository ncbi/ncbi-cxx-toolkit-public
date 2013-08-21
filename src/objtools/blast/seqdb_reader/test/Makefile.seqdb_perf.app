# $Id$

APP = seqdb_perf
SRC = seqdb_perf
LIB_ = seqdb xobjutil blastdb $(SOBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

CFLAGS    = $(FAST_CFLAGS) -fopenmp
CXXFLAGS  = $(FAST_CXXFLAGS) -fopenmp
LOCAL_LDFLAGS = -fopenmp
LDFLAGS   = $(FAST_LDFLAGS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = maning madden camacho
