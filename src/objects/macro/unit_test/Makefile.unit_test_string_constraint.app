# $Id$
APP = unit_test_string_constraint
SRC = unit_test_string_constraint

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = $(SEQ_LIBS) pub medline biblio general xser xutil macro test_boost xncbi

CHECK_CMD =

WATCHERS = 
