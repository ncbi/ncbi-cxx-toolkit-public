# $Id$

APP = test_scheduler
SRC = test_scheduler
LIB = xutil test_boost xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

PRE_LIBS = $(BOOST_LIBS)

REQUIRES = Boost.Test

CHECK_CMD =

CHECK_AUTHORS = ivanovp
