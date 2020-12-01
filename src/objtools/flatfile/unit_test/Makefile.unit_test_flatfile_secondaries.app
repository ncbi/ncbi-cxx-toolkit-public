# $Id$

APP = unit_test_flatfile_secondaries
SRC = unit_test_flatfile_secondaries

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xflatfile $(XFORMAT_LIBS) xobjutil test_boost $(OBJMGR_LIBS)
LIBS = $(PSG_CLIENT_LIBS) $(PCRE_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

WATCHERS = foleyjp
