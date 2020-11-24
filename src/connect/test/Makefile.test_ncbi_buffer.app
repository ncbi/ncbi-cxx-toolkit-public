# $Id$

APP = test_ncbi_buffer
SRC = test_ncbi_buffer
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_buffer.sh
CHECK_COPY = test_ncbi_buffer.sh


WATCHERS = lavr
