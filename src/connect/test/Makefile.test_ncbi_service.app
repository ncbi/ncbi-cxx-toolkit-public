# $Id$

APP = test_ncbi_service
SRC = test_ncbi_service
LIB = connssl connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
LINK = $(C_LINK)

CHECK_CMD = test_ncbi_service bounce

WATCHERS = lavr
