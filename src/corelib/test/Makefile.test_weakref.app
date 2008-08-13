# $Id$

APP = test_weakref
SRC = test_weakref

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = test_boost xncbi
PRE_LIBS = $(BOOST_LIBS)

REQUIRES = Boost.Test

CHECK_CMD =
