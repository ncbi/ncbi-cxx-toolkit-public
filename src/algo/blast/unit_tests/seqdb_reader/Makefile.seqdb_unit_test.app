# $Id$

APP = seqdb_unit_test
SRC = seqdb_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

PRE_LIBS = $(BOOST_LIBS)

LIB = test_boost seqdb xobjutil blastdb $(SOBJMGR_LIBS)
LIBS = $(BOOST_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD  = seqdb_unit_test
CHECK_COPY = data

REQUIRES = Boost.Test

CHECK_TIMEOUT = 300
CHECK_AUTHORS = blastsoft
