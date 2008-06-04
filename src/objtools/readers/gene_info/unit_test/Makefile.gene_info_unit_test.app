# $Id$

APP = gene_info_unit_test
SRC = gene_info_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LOCAL_LDFLAGS = -L. -L..
LDFLAGS = $(LOCAL_LDFLAGS) $(FAST_LDFLAGS)

LIB = gene_info xncbi

LIBS = $(BOOST_LIBS) $(ORIG_LIBS)

CHECK_CMD     = gene_info_unit_test
CHECK_COPY    = data
CHECK_AUTHORS = blastsoft
CHECK_REQUIRES = -IRIX
