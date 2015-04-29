# $Id$

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)

APP = test_uttp
SRC = test_uttp
LIB = xutil test_boost xncbi

REQUIRES = Boost.Test.Included

CHECK_CMD =

WATCHERS = sadyrovr
