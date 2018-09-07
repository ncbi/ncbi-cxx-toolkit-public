# $Id$
#
# Makefile:  Makefile.unit_test_discrepancy.app
#


APP = unit_test_discrepancy
SRC = unit_test_discrepancy

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)

LIB = xdiscrepancy xobjutil xcleanup valid test_boost macro $(SEQ_LIBS) $(OBJMGR_LIBS) $(OBJREAD_LIBS) xregexp xncbi

LIBS = $(PCRE_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD = unit_test_discrepancy

WATCHERS = kachalos
