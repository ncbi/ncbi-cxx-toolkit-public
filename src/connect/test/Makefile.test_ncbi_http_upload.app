# $Id$

APP = test_ncbi_http_upload
SRC = test_ncbi_http_upload

LIB = xconnect xncbi $(NCBIATOMIC_LIB)
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_http_upload.sh /CHECK_NAME=test_ncbi_http_upload
CHECK_COPY = test_ncbi_http_upload.sh ../../check/ncbi_test_data

WATCHERS = lavr
