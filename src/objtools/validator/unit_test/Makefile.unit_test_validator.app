# $Id$

APP = unit_test_validator
SRC = unit_test_validator

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = test_boost xvalidate xformat xalnmgr xobjutil valerr submit tables taxon3 gbseq $(OBJMGR_LIBS)
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =
