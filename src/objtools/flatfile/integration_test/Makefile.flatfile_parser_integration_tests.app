# $Id$

APP = flatfile_parser_integration_tests
SRC = flatfile_parser_integration_tests

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = xflatfile test_boost $(XFORMAT_LIBS) xalnmgr xobjutil taxon1 tables xregexp $(PCRE_LIB) $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(GENBANK_THIRD_PARTY_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =
CHECK_COPY = test_data

WATCHERS = foleyjp
