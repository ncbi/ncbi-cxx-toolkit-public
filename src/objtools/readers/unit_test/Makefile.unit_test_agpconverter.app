# $Id$

APP = unit_test_agpconverter
SRC = unit_test_agpconverter

LIB  = xunittestutil $(OBJREAD_LIBS) xobjutil test_boost $(SOBJMGR_LIBS)
LIBS = $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

CHECK_CMD  =
CHECK_COPY = agpconverter_test_cases

WATCHERS = bollin kornbluh
