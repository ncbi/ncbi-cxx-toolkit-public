# $Id$

APP = test_ncbi_socket
SRC = test_ncbi_socket
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_socket.sh /CHECK_NAME=test_ncbi_socket
CHECK_COPY = test_ncbi_socket.sh

WATCHERS = lavr
