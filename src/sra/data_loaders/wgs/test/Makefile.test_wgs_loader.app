# $Id$

APP = test_wgs_loader
SRC = test_wgs_loader

REQUIRES = Boost.Test.Included

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE) $(BOOST_INCLUDE)

LIB = ncbi_xloader_wgs $(SRAREAD_LIBS) xobjreadex $(OBJREAD_LIBS) xobjutil \
      test_boost $(OBJMGR_LIBS)

LIBS = $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD      = test_wgs_loader
CHECK_TIMEOUT  = 300
CHECK_REQUIRES = in-house-resources -Solaris

WATCHERS = vasilche ucko
