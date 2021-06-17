# $Id$

APP = test_ncbi_buffer
SRC = test_ncbi_buffer
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
LINK = $(C_LINK)

CHECK_CMD = test_ncbi_buffer.sh /CHECK_NAME=test_ncbi_buffer
CHECK_COPY = test_ncbi_buffer.sh

WATCHERS = lavr
