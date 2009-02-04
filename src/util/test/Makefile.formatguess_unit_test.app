# $Id$

APP = formatguess_unit_test
SRC = formatguess_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = test_boost xncbi xutil
LIBS = $(BOOST_TEST_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test
