# $Id$

APP = test_scheduler
SRC = test_scheduler

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xutil test_boost xncbi
PRE_LIBS = $(BOOST_TEST_LIBS)

REQUIRES = Boost.Test

CHECK_CMD =

CHECK_AUTHORS = ivanovp
