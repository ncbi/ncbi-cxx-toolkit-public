# $Id$

APP = test_range_set
SRC = test_range_set

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = test_boost xutil xncbi
LIBS = $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

WATCHERS = dicuccio

