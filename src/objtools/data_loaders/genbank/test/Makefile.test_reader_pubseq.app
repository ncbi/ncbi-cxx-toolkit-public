#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "serialtest"
#################################

REQUIRES = dbapi

APP = test_reader_pubseq
SRC = test_reader_pubseq

LIB = ncbi_xreader_pubseqos dbapi_driver $(COMPRESS_LIBS) $(SOBJMGR_LIBS)

LIBS = $(DL_LIBS) $(ORIG_LIBS)

