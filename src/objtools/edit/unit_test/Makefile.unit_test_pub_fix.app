# $Id$

APP = unit_test_pub_fix
SRC = unit_test_pub_fix

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)


LIB = $(OBJEDIT_LIBS) eutils esearch esummary xunittestutil xobjutil \
      test_boost $(OBJMGR_LIBS) $(PCRE_LIB)

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =

WATCHERS = foleyjp
