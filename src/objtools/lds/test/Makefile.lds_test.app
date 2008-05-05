REQUIRES = objects bdb

APP = lds_test
SRC = lds_test

LIB = ncbi_xloader_lds lds xobjread bdb xobjutil $(SOBJMGR_LIBS)
LIBS = $(BERKELEYDB_STATIC_LIBS) $(DL_LIBS) $(ORIG_LIBS)
