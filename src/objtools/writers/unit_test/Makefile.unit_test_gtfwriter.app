# $Id$

APP = unit_test_gtfwriter
SRC = unit_test_gtfwriter
LIB = xunittestutil xobjwrite $(OBJREAD_LIBS) xobjutil gbseq xalnmgr entrez2cli entrez2 \
	tables test_boost $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

#LIB  = xunittestutil xobjwrite $(OBJREAD_LIBS) xobjutil gbseq entrez2cli entrez2 test_boost $(SOBJMGR_LIBS)
#LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

REQUIRES = Boost.Test.Included

CHECK_CMD  =
CHECK_COPY = gtfwriter_test_cases

WATCHERS = ludwigf

#LIB = xobjwrite $(OBJREAD_LIBS) xobjutil gbseq xalnmgr entrez2cli entrez2 \
#	tables $(OBJMGR_LIBS)

#LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

#LIB  = xobjwrite $(OBJREAD_LIBS) $(XFORMAT_LIBS) xalnmgr xobjutil entrez2cli entrez2 tables xregexp $(PCRE_LIB) $(OBJMGR_LIBS)
#LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

