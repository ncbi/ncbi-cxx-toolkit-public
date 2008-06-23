# $Id$

APP = test_weakref
SRC = test_weakref

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xncbi test_boost
LIBS = $(ORIG_LIBS)
PRE_LIBS = $(BOOST_LIBS)

REQUIRES = Boost.Test

CHECK_CMD =
