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

CHECK_CMD = tax4blast -taxids 9606
CHECK_CMD = tax4blast -taxids 2
CHECK_CMD = tax4blast -taxids 40674
CHECK_CMD = tax4blast -taxids 9443
CHECK_CMD = tax4blast -taxids 2759
CHECK_CMD = tax4blast -taxids 2157
CHECK_CMD = tax4blast -taxids 4751
CHECK_CMD = tax4blast -taxids 562
# Right now 63221 is a leaf node taxid, therefore it has no descendant taxIDs
# (i.e.: the program's output is expected to be empty)
CHECK_CMD = tax4blast -taxids 63221

CHECK_REQUIRES = in-house-resources
CHECK_TIMEOUT = 120
