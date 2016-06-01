# $Id$

APP = test_ncbi_http_upload
SRC = test_ncbi_http_upload

LIB = xconnect xncbi $(NCBIATOMIC_LIB)
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

#LINK = purify $(ORIG_LINK)

CHECK_CMD = 
CHECK_COPY = ../../check/ncbi_test_data

WATCHERS = lavr
