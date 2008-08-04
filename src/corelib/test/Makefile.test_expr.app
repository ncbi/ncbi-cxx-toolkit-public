# $Id$

APP = test_expr
SRC = test_expr

LIB = test_boost$(STATIC) xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

PRE_LIBS = $(BOOST_LIBS)
LIBS = $(BOOST_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test

CHECK_CMD =

