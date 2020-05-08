# $Id$

APP = writedb_lmdb_unit_test
SRC = writedb_lmdb_unit_test 

CPPFLAGS = -DNCBI_MODULE=BLASTDB $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) \
           $(BLAST_THIRD_PARTY_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)

LIB_ = test_boost writedb seqdb $(OBJREAD_LIBS) xobjutil blastdb \
       $(SOBJMGR_LIBS) $(LMDB_LIB)
LIB = $(LIB_:%=%$(STATIC))
LIBS = $(BLAST_THIRD_PARTY_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_REQUIRES = in-house-resources
CHECK_CMD = writedb_lmdb_unit_test
CHECK_COPY = data

WATCHERS = madden camacho fongah2 boratyng
