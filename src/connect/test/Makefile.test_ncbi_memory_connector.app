# $Id$

APP = test_ncbi_memory_connector
SRC = test_ncbi_memory_connector
LIB = xconntest connect

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_memory_connector
