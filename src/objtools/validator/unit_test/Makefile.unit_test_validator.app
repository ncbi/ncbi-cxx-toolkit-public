# $Id$

APP = unit_test_validator
SRC = unit_test_validator wrong_qual

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = xvalidate xhugeasn taxon1 xunittestutil $(OBJEDIT_LIBS) \
       $(XFORMAT_LIBS) xalnmgr xobjutil valerr \
       tables xregexp $(PCRE_LIB) test_boost $(OBJMGR_LIBS)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =
CHECK_COPY = unit_test_validator.ini
CHECK_TIMEOUT = 3000

WATCHERS = stakhovv kans foleyjp asztalos gotvyans
