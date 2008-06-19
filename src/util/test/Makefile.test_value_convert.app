# $Id$

APP = test_value_convert
SRC = test_value_convert
LIB = xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIBS = $(BOOST_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test

CHECK_CMD =

