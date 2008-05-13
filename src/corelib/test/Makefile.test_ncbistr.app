# $Id$

APP = test_ncbistr
SRC = test_ncbistr
LIB = xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
LIBS = $(BOOST_LIBS) $(ORIG_LIBS)
REQUIRES = Boost.Test

CHECK_CMD =
