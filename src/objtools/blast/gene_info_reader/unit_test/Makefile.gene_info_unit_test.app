# $Id$

APP = gene_info_unit_test
SRC = gene_info_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(LOCAL_LDFLAGS) $(FAST_LDFLAGS)

LIB = test_boost gene_info xncbi

LIBS = $(ORIG_LIBS)

CHECK_CMD     = gene_info_unit_test
CHECK_COPY    = data
CHECK_REQUIRES = Linux

WATCHERS = madden blastsoft
