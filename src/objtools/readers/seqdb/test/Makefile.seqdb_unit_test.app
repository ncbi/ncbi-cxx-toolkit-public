# $Id$

APP = seqdb_unit_test
SRC = seqdb_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)

LIB = seqdb xobjutil blastdb $(OBJMGR_LIBS)
LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects
CHECK_CMD = seqdb_unit_test

CC_WRAPPER=ccache
CXX_WRAPPER=ccache
