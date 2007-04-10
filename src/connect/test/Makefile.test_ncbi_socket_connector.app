# $Id$

APP = test_ncbi_socket_connector
SRC = test_ncbi_socket_connector
LIB = xconntest connect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_socket_connector.sh
CHECK_COPY = test_ncbi_socket_connector.sh
