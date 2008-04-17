APP = srsearch
SRC = main

LIB_ = xalgoblastdbindex xalgoblastdbindex_search blast composition_adjustment seqdb blastdb \
      xobjread creaders tables connect $(SOBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects
