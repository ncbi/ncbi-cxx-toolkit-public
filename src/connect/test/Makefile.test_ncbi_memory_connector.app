# $Id$

APP = test_ncbi_memory_connector
SRC = test_ncbi_memory_connector ncbi_conntest
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
LINK = $(C_LINK)

CHECK_CMD =

WATCHERS = lavr
