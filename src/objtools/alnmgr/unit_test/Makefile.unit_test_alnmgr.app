# $Id$

APP = unit_test_alnmgr
SRC = unit_test_alnmgr

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xalnmgr xobjutil submit tables $(OBJMGR_LIBS) test_boost
LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_COPY = data

CHECK_CMD =

WATCHERS = grichenk
