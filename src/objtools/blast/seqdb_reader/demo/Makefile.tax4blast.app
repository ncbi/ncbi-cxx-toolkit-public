# $Id$

APP = tax4blast
SRC = tax4blast
LIB_ = seqdb sqlitewrapp xobjutil blastdb $(SOBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC)) $(LMDB_LIB)

CFLAGS    = $(FAST_CFLAGS)
CPPFLAGS += $(SQLITE3_INCLUDE)
CXXFLAGS  = $(FAST_CXXFLAGS)
LDFLAGS   = $(FAST_LDFLAGS) 

LIBS = $(SQLITE3_STATIC_LIBS) $(BLAST_THIRD_PARTY_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = madden camacho
REQUIRES = SQLITE3

CHECK_CMD = tax4blast -taxid 9606
CHECK_REQUIRES = in-house-resources
