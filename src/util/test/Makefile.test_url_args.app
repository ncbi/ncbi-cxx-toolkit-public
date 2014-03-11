# $Id$

APP = test_url_args
SRC = test_url_args
LIB = test_boost xncbi xutil

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

CHECK_CMD =

