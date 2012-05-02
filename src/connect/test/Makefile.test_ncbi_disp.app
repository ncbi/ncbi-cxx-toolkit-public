# $Id$

APP = test_ncbi_disp
SRC = test_ncbi_disp
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD =
CHECK_TIMEOUT = 30

WATCHERS = lavr
