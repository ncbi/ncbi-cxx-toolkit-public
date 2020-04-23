# $Id$

APP = unit_test_extended_cleanup
SRC = unit_test_extended_cleanup

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)


LIB = xcleanup $(OBJEDIT_LIBS) xunittestutil xalnmgr xobjutil valid gbseq \
      tables xregexp $(PCRE_LIB) test_boost $(OBJMGR_LIBS)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =

WATCHERS = bollin kans foleyjp
