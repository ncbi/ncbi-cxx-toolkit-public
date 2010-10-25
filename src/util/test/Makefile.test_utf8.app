#################################
# $Id$

APP = test_utf8
SRC = test_utf8
LIB = xutil test_boost xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

CHECK_CMD =
CHECK_COPY = test_utf8_u2a.txt

WATCHERS = gouriano
