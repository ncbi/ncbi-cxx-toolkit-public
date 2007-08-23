APP = makeindex
SRC = main mkindex_app

LIB = xalgoblastdbindex blast composition_adjustment seqdb blastdb \
      xobjread creaders tables $(SOBJMGR_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects
