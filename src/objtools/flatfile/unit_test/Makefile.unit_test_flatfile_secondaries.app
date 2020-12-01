# $Id$

APP = unit_test_flatfile_secondaries
SRC = unit_test_flatfile_secondaries

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = xflatfile test_boost $(XFORMAT_LIBS) xalnmgr xobjutil taxon1 tables xregexp $(PCRE_LIB) $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(GENBANK_THIRD_PARTY_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

WATCHERS = foleyjp
