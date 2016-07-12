# $Id$

APP = unit_test_parse_utils
SRC = unit_test_parse_utils

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) 

LIB  = xqueryparse xncbi test_boost 
LIBS = $(DL_LIBS) $(ORIG_LIBS)


REQUIRES = Boost.Test.Included

CHECK_CMD = unit_test_parse_utils
CHECK_TIMEOUT = 3000

WATCHERS = asztalos
