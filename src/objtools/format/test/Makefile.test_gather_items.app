###############################
# $Id$
###############################

APP = test_gather_items
SRC = test_gather_items

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = $(XFORMAT_LIBS) xunittestutil xobjutil xconnect $(PCRE_LIB) test_boost \
      $(SOBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

WATCHERS = foleyjp
