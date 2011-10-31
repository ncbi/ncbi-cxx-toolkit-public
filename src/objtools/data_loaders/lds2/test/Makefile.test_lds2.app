REQUIRES = objects SQLITE3

APP = test_lds2
SRC = test_lds2


LIB  = ncbi_xloader_lds2 lds2 xobjread xobjutil submit sqlitewrapp creaders xcompress $(COMPRESS_LIBS) $(SOBJMGR_LIBS)
LIBS = $(SQLITE3_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_COPY = lds2_data
CHECK_CMD  = test_lds2 -id 5
CHECK_CMD  = test_lds2 -gzip -id 5
CHECK_CMD  = test_lds2 -stress
CHECK_CMD  = test_lds2 -stress -gzip -format fasta

WATCHERS = grichenk
