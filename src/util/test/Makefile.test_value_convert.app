# $Id$

APP = test_value_convert
SRC = test_value_convert
LIB = test_boost xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

CHECK_CMD =


WATCHERS = ucko
