# $Id$

APP = test_scheduler
SRC = test_scheduler
LIB = xncbi xutil

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(ORIG_LIBS)

CHECK_CMD =

CHECK_AUTHORS = ivanovp
