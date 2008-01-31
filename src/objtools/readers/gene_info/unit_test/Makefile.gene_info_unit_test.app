# $Id$

APP = gene_info_unit_test
SRC = gene_info_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LOCAL_LDFLAGS = -L. -L..
LDFLAGS = $(LOCAL_LDFLAGS) $(FAST_LDFLAGS)

LIB_ = gene_info xobjutil xobjsimple $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(BOOST_LIBPATH) $(BOOST_TEST_UTF_LIBS) \
       $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
       

CHECK_CMD = gene_info_unit_test
CHECK_COPY = data
CHECK_AUTHORS = blastsoft
