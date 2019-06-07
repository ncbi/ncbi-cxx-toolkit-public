# $Id$

APP = seqdb_unit_test
SRC = seqdb_unit_test

CPPFLAGS = -DNCBI_MODULE=BLASTDB $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) $(BLAST_THIRD_PARTY_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIB = test_boost seqdb writedb xobjutil blastdb $(OBJREAD_LIBS) $(SOBJMGR_LIBS) $(LMDB_LIB)
LIBS = $(BLAST_THIRD_PARTY_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_REQUIRES = in-house-resources
CHECK_CMD  = seqdb_unit_test
CHECK_COPY = seqdb_unit_test.ini data

REQUIRES = Boost.Test.Included

CHECK_TIMEOUT = 700

WATCHERS = madden camacho
