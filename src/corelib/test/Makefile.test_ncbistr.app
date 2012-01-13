# $Id$

APP = test_ncbistr
SRC = test_ncbistr
LIB = test_boost xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

CHECK_CMD  = test_ncbistr.sh
CHECK_COPY = test_ncbistr.sh

WATCHERS = ivanov
