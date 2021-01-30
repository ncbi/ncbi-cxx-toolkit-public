# $Id$

APP = test_ncbi_service_connector
SRC = test_ncbi_service_connector
LIB = connssl connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
LINK = $(C_LINK)

CHECK_CMD =

WATCHERS = lavr
