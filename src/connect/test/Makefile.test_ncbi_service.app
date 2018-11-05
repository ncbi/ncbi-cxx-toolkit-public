# $Id$

APP = test_ncbi_service
SRC = test_ncbi_service
LIB = connect connssl $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_service bounce

WATCHERS = lavr
