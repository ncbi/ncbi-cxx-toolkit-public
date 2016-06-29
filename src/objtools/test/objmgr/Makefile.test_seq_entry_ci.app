# $Id$

APP = test_seq_entry_ci
SRC = test_seq_entry_ci

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = test_boost $(OBJMGR_LIBS)
LIBS = $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =

WATCHERS = vasilche grichenk elisovdn
