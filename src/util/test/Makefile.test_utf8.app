#################################
# $Id$

APP = test_utf8
SRC = test_utf8
LIB = xutil test_boost xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
PRE_LIBS = $(BOOST_TEST_LIBS)

REQUIRES = Boost.Test

CHECK_CMD =
