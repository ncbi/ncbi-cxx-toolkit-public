APP = phytree_format_unit_test
SRC = phytree_format_unit_test

LIB = xalgoalignnw phytree_format xalgophytree fastme biotree test_boost \
      $(BLAST_INPUT_LIBS) $(BLAST_LIBS) $(OBJREAD_LIBS) xobjutil \
      $(OBJMGR_LIBS)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(BLAST_THIRD_PARTY_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

CHECK_CMD = phytree_format_unit_test
CHECK_COPY = data phytree_format_unit_test.ini

WATCHERS = blastsoft
