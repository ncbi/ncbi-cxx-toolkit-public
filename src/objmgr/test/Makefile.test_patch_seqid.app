# $Id$

APP = test_patch_seqid
SRC = test_patch_seqid

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = $(SOBJMGR_LIBS)
LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test
