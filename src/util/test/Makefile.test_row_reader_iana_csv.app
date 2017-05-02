# $Id$

APP = test_row_reader_iana_csv
SRC = test_row_reader_iana_csv

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = test_boost xncbi

REQUIRES = Boost.Test.Included

CHECK_CMD =
CHECK_COPY =

WATCHERS = satskyse
