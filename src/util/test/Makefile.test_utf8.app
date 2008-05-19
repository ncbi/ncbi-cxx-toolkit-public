#################################
# $Id$

APP = test_utf8
SRC = test_utf8
LIB = test_boost xutil xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
LIBS = $(ORIG_LIBS)
PRE_LIBS = $(BOOST_LIBS)
REQUIRES = Boost.Test

CHECK_CMD =
