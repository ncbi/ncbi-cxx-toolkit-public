#################################
# $Id$

APP = test_utf8
SRC = test_utf8
LIB = xutil xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(ORIG_LIBS)
REQUIRES = Boost.Test

CHECK_CMD =
