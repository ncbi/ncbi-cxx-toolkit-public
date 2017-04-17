# $Id$

APP = test_ncbi_namerd
SRC = test_ncbi_namerd

LIB = connect $(NCBIATOMIC_LIB)
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_ncbi_namerd.sh
CHECK_COPY = test_ncbi_namerd.sh
CHECK_TIMEOUT = 30

WATCHERS = lavr mcelhany
