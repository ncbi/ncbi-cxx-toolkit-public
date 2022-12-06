# $Id$

APP = test_ncbimtx
SRC = test_ncbimtx
LIB = test_boost xncbi

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included MT

CHECK_CMD =
CHECK_COPY = test_ncbimtx.ini

WATCHERS = sadyrovr
