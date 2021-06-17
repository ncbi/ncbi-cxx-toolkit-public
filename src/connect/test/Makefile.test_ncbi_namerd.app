# $Id$

APP = test_ncbi_namerd
SRC = test_ncbi_namerd

LIB = connect $(NCBIATOMIC_LIB)
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
LINK = $(C_LINK)

CHECK_REQUIRES = in-house-resources
CHECK_CMD = test_ncbi_namerd.sh /CHECK_NAME=test_ncbi_namerd
CHECK_COPY = test_ncbi_namerd.sh ../../check/ncbi_test_data
CHECK_TIMEOUT = 30

WATCHERS = lavr
