# $Id$

APP = test_weakref
SRC = test_weakref

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = test_boost xncbi

REQUIRES = Boost.Test.Included

CHECK_CMD =

WATCHERS = satskyse
