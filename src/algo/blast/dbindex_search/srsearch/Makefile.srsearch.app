APP = srsearch
SRC = main srsearch_app

LIB_ = xalgoblastdbindex xalgoblastdbindex_search blast composition_adjustment seqdb blastdb \
      $(OBJREAD_LIBS) xobjutil tables connect $(SOBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC)) $(LMDB_LIB)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(BLAST_THIRD_PARTY_LIBS) $(ORIG_LIBS)

REQUIRES = objects

WATCHERS = morgulis
