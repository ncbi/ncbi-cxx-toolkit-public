# $Id$
#
# Makefile:  Makefile.unit_test_discrepancy.app
#


APP = unit_test_discrepancy
SRC = unit_test_discrepancy

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)

LIB = xdiscrepancy test_boost macro xncbi

LIBS = $(PCRE_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD = unit_test_discrepancy

WATCHERS = kachalos
