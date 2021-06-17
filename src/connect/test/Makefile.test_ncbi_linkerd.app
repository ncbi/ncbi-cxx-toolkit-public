# $Id$

# Temporarily disable on Windows due to missing devops support.
REQUIRES = -MSWin

APP = test_ncbi_linkerd
SRC = test_ncbi_linkerd

LIB = connssl connect $(NCBIATOMIC_LIB)
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
LINK = $(C_LINK)

CHECK_REQUIRES = in-house-resources
CHECK_CMD = test_ncbi_linkerd.sh /CHECK_NAME=test_ncbi_linkerd
CHECK_COPY = test_ncbi_linkerd.sh ../../check/ncbi_test_data
CHECK_TIMEOUT = 30

WATCHERS = lavr
