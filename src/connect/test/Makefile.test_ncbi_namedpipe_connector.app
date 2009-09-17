# $Id$

APP = test_ncbi_namedpipe_connector
SRC = test_ncbi_namedpipe_connector
LIB = xconntest xconnect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS) 

CHECK_REQUIRES = -Cygwin

CHECK_CMD = test_ncbi_namedpipe_connector.sh
CHECK_COPY = test_ncbi_namedpipe_connector.sh

WATCHERS = lavr
