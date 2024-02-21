# $Id$

APP = tax4blast
SRC = tax4blast
LIB_ = seqdb sqlitewrapp xobjutil blastdb $(SOBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC)) $(LMDB_LIB)

CFLAGS    = $(FAST_CFLAGS)
CPPFLAGS += $(SQLITE3_INCLUDE)
CXXFLAGS  = $(FAST_CXXFLAGS)
LDFLAGS   = $(FAST_LDFLAGS) 

LIBS = $(BLAST_THIRD_PARTY_LIBS) $(SQLITE3_STATIC_LIBS) $(CMPRS_LIBS) \
       $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = camacho
REQUIRES = SQLITE3

# Human
CHECK_CMD = tax4blast -taxids 9606
# Bacteria
CHECK_CMD = tax4blast -taxids 2
# mammals
CHECK_CMD = tax4blast -taxids 40674
# primates
CHECK_CMD = tax4blast -taxids 9443
# Eukaryota
CHECK_CMD = tax4blast -taxids 2759
# Archaea
CHECK_CMD = tax4blast -taxids 2157
# Fungi
CHECK_CMD = tax4blast -taxids 4751
# Escherichia coli
CHECK_CMD = tax4blast -taxids 562

# Neandertal: right now 63221 is a leaf node taxid, therefore it has no descendant taxIDs
# (i.e.: the program's output is expected to be empty)
CHECK_CMD = tax4blast -taxids 63221

CHECK_REQUIRES = in-house-resources
CHECK_TIMEOUT = 120
