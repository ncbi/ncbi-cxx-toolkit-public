# $Id$

APP = test_scheduler
SRC = test_scheduler
LIB = xncbi xutil

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIBS = $(BOOST_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test

CHECK_CMD =

CHECK_AUTHORS = ivanovp
