# $Id$

APP = test_ncbi_socket_connector
SRC = test_ncbi_socket_connector
LIB = xconntest connect

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_ncbi_socket_connector.sh
CHECK_COPY = test_ncbi_socket_connector.sh
