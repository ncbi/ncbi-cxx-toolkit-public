# $Id$

APP = test_scheduler
SRC = test_scheduler
LIB = xncbi xutil test_boost

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIBS = $(ORIG_LIBS)
PRE_LIBS = $(BOOST_LIBS)

REQUIRES = Boost.Test

CHECK_CMD =

CHECK_AUTHORS = ivanovp
