# $Id$

APP = test_ncbi_buffer
SRC = test_ncbi_buffer
LIB = connect

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = echo "test string" | ./test_ncbi_buffer
