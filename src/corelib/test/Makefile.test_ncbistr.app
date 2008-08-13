# $Id$

APP = test_ncbistr
SRC = test_ncbistr
LIB = test_boost xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
PRE_LIBS = $(BOOST_LIBS)
REQUIRES = Boost.Test

CHECK_CMD =
