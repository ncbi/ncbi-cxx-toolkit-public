# $Id$


REQUIRES = dbapi

APP = test_reader_id1
SRC = test_reader_id1
LIB = ncbi_xreader_id1 xconnect $(COMPRESS_LIBS) $(SOBJMGR_LIBS)

LIBS = $(DL_LIBS) $(ORIG_LIBS)

