# $Id$
APP = field_collection_unit_test
SRC = field_collection_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = $(SEQ_LIBS) pub medline biblio general xser xutil test_boost xncbi
LIBS = $(UUID_LIBS)

CHECK_CMD = field_collection_unit_test -data-in uo.asnt
CHECK_COPY = uo.asnt

WATCHERS = whlavina
