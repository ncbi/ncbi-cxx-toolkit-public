# $Id$

APP = test_ncbi_memory_connector
SRC = test_ncbi_memory_connector
LIB = connect

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_ncbi_memory_connector test_ncbi_memory_connector
