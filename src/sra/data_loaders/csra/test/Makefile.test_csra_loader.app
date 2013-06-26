APP = test_csra_loader
SRC = test_csra_loader

REQUIRES = Boost.Test.Included

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = ncbi_xloader_csra $(SRAREAD_LIBS) xobjreadex submit xobjutil test_boost $(OBJMGR_LIBS)

LIBS = $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_COPY = mapfile
CHECK_CMD = test_csra_loader
CHECK_REQUIRES = in-house-resources -Solaris

WATCHERS = vasilche ucko
