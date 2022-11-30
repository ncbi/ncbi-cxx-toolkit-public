# $Id$
APP = unit_test_dbtag
SRC = unit_test_dbtag

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = general xser xutil test_boost xncbi

CHECK_CMD = unit_test_dbtag

WATCHERS = gotvyans
