APP = test_sra_loader
SRC = test_sra_loader

REQUIRES = Boost.Test.Included

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = ncbi_xloader_sra $(SRAREAD_LIBS) test_boost $(SOBJMGR_LIBS) $(CMPRS_LIB)

LIBS = $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

CHECK_CMD = test_sra_loader
CHECK_REQUIRES = in-house-resources -MSWin -Solaris

WATCHERS = vasilche ucko
