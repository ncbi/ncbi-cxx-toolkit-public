# $Id$

APP = test_ncbi_namedpipe
SRC = test_ncbi_namedpipe
LIB = xconnect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_ncbi_namedpipe.sh /CHECK_NAME=test_ncbi_namedpipe
CHECK_COPY = test_ncbi_namedpipe.sh

WATCHERS = ivanov lavr
