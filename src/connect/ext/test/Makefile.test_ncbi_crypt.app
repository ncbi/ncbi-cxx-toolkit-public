# $Id$

APP = test_ncbi_crypt
SRC = test_ncbi_crypt
LIB = connext connect

LIBS = $(NETWORK_LIBS) $(C_LIBS)
LINK = $(C_LINK)
#LINK = purify $(C_LINK)

CHECK_CMD =
CHECK_TIMEOUT = 360

WATCHERS = lavr
