# $Id$

APP = test_ncbi_namedpipe_connector
SRC = test_ncbi_namedpipe_connector ncbi_conntest
LIB = xconnect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS) 

CHECK_CMD = test_ncbi_namedpipe_connector.sh /CHECK_NAME=test_ncbi_namedpipe_connector
CHECK_COPY = test_ncbi_namedpipe_connector.sh

WATCHERS = lavr
