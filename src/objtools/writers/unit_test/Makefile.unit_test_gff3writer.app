# $Id$

APP = unit_test_gff3writer
SRC = unit_test_gff3writer
LIB = xunittestutil xobjwrite variation_utils $(OBJEDIT_LIBS) $(OBJREAD_LIBS) xalnmgr xobjutil gbseq entrez2cli entrez2 \
        tables test_boost $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

CHECK_CMD  =
CHECK_COPY = gff3writer_test_cases

WATCHERS = ludwigf
