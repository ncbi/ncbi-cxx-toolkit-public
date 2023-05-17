# $Id$

APP = test_ncbi_ifconf
SRC = test_ncbi_ifconf
LIB = connext connect $(NCBIATOMIC_LIB)

REQUIRES = unix

LIBS = $(NETWORK_LIBS) $(C_LIBS)
LINK = $(C_LINK)
# LINK = purify $(C_LINK)

CHECK_REQUIRES = unix
CHECK_CMD =

WATCHERS = lavr
