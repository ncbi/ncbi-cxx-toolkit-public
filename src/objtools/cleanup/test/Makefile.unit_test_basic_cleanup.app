# $Id$

APP = unit_test_basic_cleanup
SRC = unit_test_basic_cleanup

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xcleanup xobjutil valid submit xregexp $(PCRE_LIB) $(COMPRESS_LIBS) \
      test_boost $(SOBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =
CHECK_COPY = test_cases
CHECK_TIMEOUT = 1200

WATCHERS = bollin kornbluh
