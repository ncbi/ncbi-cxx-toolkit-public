APP = makembindex
SRC = main mkindex_app

LIB_ = xalgoblastdbindex blast composition_adjustment seqdb blastdb \
      $(OBJREAD_LIBS) xobjutil tables connect $(SOBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC)) $(LMDB_LIB)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIBS =  $(BLAST_THIRD_PARTY_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects

WATCHERS = morgulis
