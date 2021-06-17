# $Id$

APP = bam_unit_test
SRC = bam_unit_test

REQUIRES = objects Boost.Test.Included

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE) $(BOOST_INCLUDE)

LIB = bamread $(BAM_LIBS) $(OBJREAD_LIBS) test_boost $(OBJMGR_LIBS)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

CHECK_CMD = bam_unit_test
CHECK_REQUIRES = in-house-resources -Solaris

WATCHERS = vasilche
