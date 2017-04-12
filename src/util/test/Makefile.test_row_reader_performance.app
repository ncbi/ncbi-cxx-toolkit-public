# $Id$

APP = test_row_reader_performance
SRC = test_row_reader_performance

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = xncbi

# REQUIRES = Boost.Test.Included

# CHECK_CMD =

WATCHERS = satskyse
