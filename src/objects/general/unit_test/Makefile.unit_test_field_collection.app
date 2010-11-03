# $Id$
APP = unit_test_field_collection
SRC = unit_test_field_collection

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = $(SEQ_LIBS) general xser xutil test_boost xncbi
LIBS = $(UUID_LIBS)

CHECK_CMD = unit_test_field_collection -data-in uo.asnt
CHECK_COPY = uo.asnt

WATCHERS = whlavina
