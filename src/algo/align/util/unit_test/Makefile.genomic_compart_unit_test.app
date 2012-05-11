# $Id$

APP = genomic_compart_unit_test
SRC = genomic_compart_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xalgoalignutil xalgoseq xalnmgr xqueryparse tables \
	  taxon1 xregexp $(PCRE_LIB) $(BLAST_LIBS) \
	  test_boost $(OBJMGR_LIBS)

LIBS = $(NETWORK_LIBS) $(PCRE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included objects

CHECK_CMD = genomic_compart_unit_test -input-dir data -expected-results data/genomic_compart.test.results
CHECK_COPY = data


WATCHERS = mozese2
