# $Id$

APP = test_ncbistr
SRC = test_ncbistr
LIB = xncbi test_boost

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
LIBS = $(ORIG_LIBS)
PRE_LIBS = $(BOOST_LIBS)
REQUIRES = Boost.Test

CHECK_CMD =
