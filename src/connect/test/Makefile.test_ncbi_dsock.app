# $Id$

APP = test_ncbi_dsock
SRC = test_ncbi_dsock
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
LINK = $(C_LINK)

CHECK_CMD = test_ncbi_dsock.sh /CHECK_NAME=test_ncbi_dsock
CHECK_COPY = test_ncbi_dsock.sh

WATCHERS = lavr
