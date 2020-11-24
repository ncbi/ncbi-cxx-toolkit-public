# $Id$

APP = test_ncbi_core
SRC = test_ncbi_core
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD =

WATCHERS = lavr
