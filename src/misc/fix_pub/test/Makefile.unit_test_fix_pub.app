# $Id$

APP = unit_test_fix_pub
SRC = unit_test_fix_pub

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)


LIB = fix_pub $(OBJEDIT_LIBS) xunittestutil xobjutil \
      eutils_client test_boost $(OBJMGR_LIBS) xmlwrapp $(PCRE_LIB)

LIBS = $(PCRE_LIBS) $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =

WATCHERS = dobronad foleyjp
