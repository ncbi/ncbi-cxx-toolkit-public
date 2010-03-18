APP = makembindex
SRC = main mkindex_app

LIB_ = xalgoblastdbindex blast composition_adjustment seqdb blastdb \
      xobjread creaders xobjutil tables connect $(SOBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects

WATCHERS = morgulis
