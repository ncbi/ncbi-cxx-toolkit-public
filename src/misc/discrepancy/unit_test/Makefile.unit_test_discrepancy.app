# $Id$
#
# Makefile:  Makefile.unit_test_discrepancy.app
#


APP = unit_test_discrepancy
SRC = unit_test_discrepancy

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)

LIB = xdiscrepancy xcleanup xobjutil $(OBJEDIT_LIBS) taxon3 valid test_boost macro $(SEQ_LIBS) $(OBJMGR_LIBS) $(OBJREAD_LIBS) $(CMPRS_LIB) xregexp xncbi

LIBS =  $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD = unit_test_discrepancy

WATCHERS = kachalos
