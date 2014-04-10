# $Id$

APP = unit_test_extended_cleanup
SRC = unit_test_extended_cleanup

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xcleanup xunittestutil xobjutil valid submit xregexp $(PCRE_LIB) \
      test_boost $(SOBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =

WATCHERS = bollin kornbluh
