# $Id$

APP = test_metaphone
SRC = test_metaphone

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = test_boost xutil xncbi
LIBS = $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_metaphone -data-in metaphone-data.txt
CHECK_COPY = metaphone-data.txt

REQUIRES = Boost.Test.Included

WATCHERS = dicuccio

