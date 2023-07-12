
APP = seqdb_tax4blast_unit_test
SRC = seqdb_tax4blast_unit_test

CPPFLAGS = -DNCBI_MODULE=BLASTDB $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) $(SQLITE3_INCLUDE) $(BLAST_THIRD_PARTY_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIB = test_boost seqdb writedb sqlitewrapp xobjutil blastdb $(OBJREAD_LIBS) $(SOBJMGR_LIBS) $(LMDB_LIB)
LIBS = $(SQLITE3_STATIC_LIBS) $(BLAST_THIRD_PARTY_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD  = seqdb_tax4blast_unit_test
CHECK_COPY = data

REQUIRES = Boost.Test.Included SQLITE3

CHECK_TIMEOUT = 600

WATCHERS = madden camacho fongah2
