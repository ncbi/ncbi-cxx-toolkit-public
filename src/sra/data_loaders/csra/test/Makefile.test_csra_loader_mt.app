APP = test_csra_loader_mt
SRC = test_csra_loader_mt

LIB = ncbi_xloader_csra $(SRAREAD_LIBS) xobjreadex $(OBJREAD_LIBS) xobjutil \
      test_mt $(OBJMGR_LIBS)

LIBS = $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

CHECK_CMD = test_csra_loader_mt -threads 24 /CHECK_NAME=test_csra_loader_mt
CHECK_REQUIRES = in-house-resources -Solaris

WATCHERS = vasilche
