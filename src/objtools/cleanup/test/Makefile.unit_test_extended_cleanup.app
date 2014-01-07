# $Id$

APP = unit_test_extended_cleanup
SRC = unit_test_extended_cleanup

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = test_boost xcleanup xunittestutil xregexp $(XFORMAT_LIBS) xalnmgr xobjutil valid valerr submit tables gbseq $(OBJMGR_LIBS) $(PCRE_LIB)
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(PCRE_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =

WATCHERS = bollin kornbluh
