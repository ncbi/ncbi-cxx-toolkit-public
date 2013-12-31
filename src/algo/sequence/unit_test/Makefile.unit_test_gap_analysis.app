# $Id$

APP = unit_test_gap_analysis

SRC = unit_test_gap_analysis

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = xalgoseq xvalidate xobjedit $(OBJREAD_LIBS) valid valerr taxon3 taxon1 $(XFORMAT_LIBS) xalnmgr xobjutil tables xregexp $(PCRE_LIB) test_boost $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

# Uncomment if you do not want it to run automatically as part of
# "make check".
CHECK_CMD = unit_test_gap_analysis -in-data gap_analysis.seq.asn
CHECK_COPY = gap_analysis.seq.asn

WATCHERS = kornbluh
