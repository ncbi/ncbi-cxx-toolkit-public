# $Id$

APP = unit_test_linkage_evidence
SRC = unit_test_linkage_evidence

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = $(SEQ_LIBS) xutil test_boost xncbi

LIBS = $(DL_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

CHECK_CMD =

WATCHERS = foleyjp
