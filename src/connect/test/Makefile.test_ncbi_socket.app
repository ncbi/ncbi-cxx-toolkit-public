# $Id$

APP = test_ncbi_socket
SRC = test_ncbi_socket
LIB = connect

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_ncbi_socket.sh
CHECK_COPY = test_ncbi_socket.sh
