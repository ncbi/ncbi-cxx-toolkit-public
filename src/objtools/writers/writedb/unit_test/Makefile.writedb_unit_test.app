# $Id$

APP = writedb_unit_test
SRC = writedb_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)

LIB_ = seqdb writedb xobjutil xobjread creaders blastdb $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))
LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) \
$(DL_LIBS) $(ORIG_LIBS)

REQUIRES = dbapi
CHECK_CMD = writedb_unit_test
CHECK_AUTHORS = blastsoft
