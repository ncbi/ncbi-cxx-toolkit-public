APP = makeindex
SRC = main mkindex_app

LIB = xalgoblastdbindex blast composition_adjustment seqdb blastdb \
      xobjread tables $(OBJMGR_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(ORIG_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS)

REQUIRES = objects
