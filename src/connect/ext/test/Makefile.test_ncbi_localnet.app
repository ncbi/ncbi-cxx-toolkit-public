# $Id$

APP = test_ncbi_localnet
SRC = test_ncbi_localnet
LIB = connext connssl connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(C_LIBS)
LINK = $(C_LINK)
# LINK = purify $(C_LINK)

CHECK_CMD = test_ncbi_localnet.sh /CHECK_NAME=test_ncbi_localnet
CHECK_COPY = test_ncbi_localnet.sh

WATCHERS = lavr
