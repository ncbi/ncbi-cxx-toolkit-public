# $Id$

APP = test_connect_misc
SRC = test_connect_misc
LIB = xconnect test_boost xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = MT Boost.Test.Included

CHECK_CMD =

WATCHERS = sadyrovr
