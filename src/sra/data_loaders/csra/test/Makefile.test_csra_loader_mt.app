APP = test_csra_loader_mt
SRC = test_csra_loader_mt

LIB = ncbi_xloader_csra $(SRAREAD_LIBS) xobjreadex $(OBJREAD_LIBS) xobjutil \
      test_mt $(OBJMGR_LIBS)

LIBS = $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

CHECK_CMD = test_csra_loader_mt -threads 24 /CHECK_NAME=test_csra_loader_mt
CHECK_CMD = test_csra_loader_mt -threads 24 -reference-sequences -accs SRR000010,SRR389414,SRR494733,SRR505887,SRR035417 /CHECK_NAME=test_csra_loader_mt_ref
CHECK_REQUIRES = in-house-resources

WATCHERS = vasilche
