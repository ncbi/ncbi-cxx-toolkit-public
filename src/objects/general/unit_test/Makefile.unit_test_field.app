# $Id$
APP = unit_test_field
SRC = unit_test_field

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = general xser xutil test_boost xncbi

CHECK_CMD = unit_test_field

WATCHERS = vasilche
