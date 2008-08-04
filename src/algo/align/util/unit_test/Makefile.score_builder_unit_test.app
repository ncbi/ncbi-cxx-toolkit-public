# $Id$

APP = algoalignutil_unit_test
SRC = score_builder_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB_ = xalgoalignutil xalnmgr $(BLAST_LIBS) $(OBJMGR_LIBS) \
	xutil xncbi test_boost connext tables
LIB = $(LIB_:%=%$(STATIC))
PRE_LIBS = $(BOOST_LIBS)

LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

REQUIRES = objects

CHECK_CMD = algoalignutil_unit_test
CHECK_COPY = data
