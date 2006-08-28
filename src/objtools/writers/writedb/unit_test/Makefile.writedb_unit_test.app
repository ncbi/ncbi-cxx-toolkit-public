# $Id$

APP = writedb_unit_test
SRC = writedb_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xncbi seqdb writedb xobjutil blastdb $(OBJMGR_LIBS)
LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects
CHECK_CMD = writedb_unit_test
CHECK_TIMEOUT = 60

