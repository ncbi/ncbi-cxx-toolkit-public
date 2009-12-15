# $Id$

APP = test_align
SRC = test_align

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = test_boost xncbi

REQUIRES = Boost.Test.Included

CHECK_CMD =

WATCHERS = todorov
