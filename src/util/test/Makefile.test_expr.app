# $Id$

APP = test_expr
SRC = test_expr
LIB = xutil xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIBS = $(BOOST_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test

CHECK_CMD =

