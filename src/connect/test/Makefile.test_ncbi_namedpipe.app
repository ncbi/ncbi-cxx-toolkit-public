# $Id$

APP = test_ncbi_namedpipe
SRC = test_ncbi_namedpipe
LIB = xconnect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_ncbi_namedpipe.sh
CHECK_COPY = test_ncbi_namedpipe.sh
