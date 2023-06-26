#################################
# $Id$

APP = test_ctre
SRC = test_ctre
LIB = xutil test_boost xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included -ICC

CHECK_CMD =

WATCHERS = gotvyans
