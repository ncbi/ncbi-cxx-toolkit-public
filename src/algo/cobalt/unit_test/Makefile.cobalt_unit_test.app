# $Id$

APP = cobalt_unit_test
SRC = cobalt_unit_test options_unit_test kmer_unit_test clusterer_unit_test \
      links_unit_test seq_unit_test cobalt_test_util

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIB = test_boost xncbi cobalt xalgoalignnw xalgophytree fastme biotree ncbi_xloader_blastdb \
      xalnmgr $(BLAST_LIBS) $(OBJMGR_LIBS)

LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD = cobalt_unit_test
CHECK_COPY = data

WATCHERS = boratyng
