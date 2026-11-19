# $Id$

APP = test_insdc_aa_utils
SRC = test_insdc_aa_utils
LIB = xunittestutil xobjwrite variation_utils $(OBJREAD_LIBS) xalnmgr xobjutil gbseq entrez2cli entrez2 \
	tables test_boost $(OBJMGR_LIBS)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

CHECK_CMD  =

WATCHERS = foleyj

