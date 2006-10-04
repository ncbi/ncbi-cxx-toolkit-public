# $Id$

APP = seqalignfilter_unit_test
SRC = seqalignfilter_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)

LIB = xblastformat xblast xalgodustmask xnetblastcli xnetblast xobjutil xobjsimple scoremat seqdb blastdb tables $(OBJMGR_LIBS)
LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects
CHECK_CMD = seqalignfilter_unit_test
