# $Id$

APP = test_ncbi_server_info
SRC = test_ncbi_server_info
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
LINK = $(C_LINK)

CHECK_CMD = test_ncbi_server_info.sh /CHECK_NAME=test_ncbi_server_info
CHECK_COPY = test_ncbi_server_info.sh

WATCHERS = lavr
