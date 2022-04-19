# $Id$

APP = test_ncbicgi
SRC = test_ncbicgi
LIB = test_boost xcgi xutil xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

CHECK_CMD  =

WATCHERS = sadyrovr
