# $Id$

APP = test_ncbi_socket
SRC = test_ncbi_socket
LIB = connect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_socket.sh
CHECK_COPY = test_ncbi_socket.sh
