# $Id$

APP = test_ncbi_file_connector
SRC = test_ncbi_file_connector
LIB = xconntest connect

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_ncbi_file_connector test_ncbi_file_connector
