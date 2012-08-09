# $Id$

APP = test_porter_stemming
SRC = test_porter_stemming

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = test_boost xutil xncbi
LIBS = $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_porter_stemming -data-in porter-data.txt
CHECK_COPY = porter-data.txt

REQUIRES = Boost.Test.Included

WATCHERS = dicuccio

