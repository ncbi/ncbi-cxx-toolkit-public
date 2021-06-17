# $Id$

APP = test_ncbi_connutil_misc
SRC = test_ncbi_connutil_misc
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(C_LIBS)
LINK = $(C_LINK)
#LINK = purify $(C_LINK)

CHECK_CMD =

WATCHERS = lavr
