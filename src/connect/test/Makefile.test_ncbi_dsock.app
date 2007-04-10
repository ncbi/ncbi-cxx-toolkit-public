# $Id$

APP = test_ncbi_dsock
SRC = test_ncbi_dsock
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_dsock.sh
CHECK_COPY = test_ncbi_dsock.sh
