# $Id$

APP = bamread_unit_test
SRC = bamread_unit_test

REQUIRES = objects Boost.Test.Included

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE) $(BOOST_INCLUDE)

LIB = bamread $(BAM_LIBS) seqset seq pub medline biblio seqcode general test_boost sequtil xser $(COMPRESS_LIBS) xutil xncbi

LIBS = $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

CHECK_CMD = bamread_unit_test
CHECK_REQUIRES = in-house-resources

WATCHERS = vasilche
