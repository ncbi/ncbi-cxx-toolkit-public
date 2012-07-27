# $Id$

APP = test_scheduler
SRC = test_scheduler

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xutil test_boost xncbi

REQUIRES = Boost.Test.Included

CHECK_CMD =


WATCHERS = vakatov
