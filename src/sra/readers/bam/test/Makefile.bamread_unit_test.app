# $Id$

APP = bamread_unit_test
SRC = bamread_unit_test

REQUIRES = objects Boost.Test.Included

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE) $(BOOST_INCLUDE)

LIB = bamread $(BAM_LIBS) test_boost xncbi

LIBS = $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

CHECK_CMD = bamread_unit_test
CHECK_REQUIRES = in-house-resources

WATCHERS = vasilche
