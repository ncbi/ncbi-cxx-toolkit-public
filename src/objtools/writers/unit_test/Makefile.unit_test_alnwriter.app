# $Id$

APP = unit_test_alnwriter
SRC = unit_test_alnwriter
LIB = xunittestutil xobjwrite variation_utils $(OBJEDIT_LIBS) $(OBJREAD_LIBS) xalnmgr xobjutil gbseq entrez2cli entrez2 \
        tables test_boost $(OBJMGR_LIBS)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

CHECK_CMD  =
CHECK_COPY = alnwriter_test_cases

WATCHERS = foleyjp
