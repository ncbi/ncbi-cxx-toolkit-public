# $Id$

PROJ_TAG = test

APP = test_wgs_loader_mt
SRC = test_wgs_loader_mt

REQUIRES = Boost.Test.Included MT

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE) $(BOOST_INCLUDE)

LIB = ncbi_xloader_wgs $(SRAREAD_LIBS) xobjreadex $(OBJREAD_LIBS) xobjutil \
      test_boost $(OBJMGR_LIBS)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

CHECK_COPY     = test_wgs_loader_mt.ini
CHECK_CMD      = test_wgs_loader_mt
CHECK_TIMEOUT  = 300
CHECK_REQUIRES = 

WATCHERS = vasilche ucko
