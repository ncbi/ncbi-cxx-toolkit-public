# $Id$

APP = test_ncbi_disp
SRC = test_ncbi_disp
LIB = connssl connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
LINK = $(C_LINK)
#LINK = purify $(C_LINK)

CHECK_CMD =
CHECK_TIMEOUT = 30

WATCHERS = lavr
