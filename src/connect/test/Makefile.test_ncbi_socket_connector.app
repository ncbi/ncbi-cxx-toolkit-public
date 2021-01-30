# $Id$

APP = test_ncbi_socket_connector
SRC = test_ncbi_socket_connector ncbi_conntest
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
LINK = $(C_LINK)

CHECK_CMD = test_ncbi_socket_connector.sh /CHECK_NAME=test_ncbi_socket_connector
CHECK_COPY = test_ncbi_socket_connector.sh

WATCHERS = lavr
