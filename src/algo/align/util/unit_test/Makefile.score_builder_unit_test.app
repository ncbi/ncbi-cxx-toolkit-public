# $Id$

APP = score_builder_unit_test
SRC = score_builder_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xalgoalignutil xalgoseq xalnmgr xqueryparse tables \
	  taxon1 xregexp $(PCRE_LIB) $(BLAST_LIBS) \
	  test_boost $(OBJMGR_LIBS)

LIBS = $(NETWORK_LIBS) $(PCRE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included objects

CHECK_CMD = score_builder_unit_test -data-in data/seqalign.asn
CHECK_COPY = data


WATCHERS = dicuccio
