# $Id$

APP = test_align
SRC = test_align

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = xncbi
LIBS = $(BOOST_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test
