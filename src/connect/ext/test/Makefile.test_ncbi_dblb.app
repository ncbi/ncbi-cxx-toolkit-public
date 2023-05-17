# $Id$

APP = test_ncbi_dblb
SRC = test_ncbi_dblb
LIB = connext connssl connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(C_LIBS)
LINK = $(C_LINK)
# LINK = purify $(C_LINK)

CHECK_REQUIRES = in-house-resources
CHECK_COPY = test_ncbi_dblb.py test_ncbi_dblb.sh
CHECK_CMD = test_ncbi_dblb.sh /CHECK_NAME=test_ncbi_dblb

WATCHERS = lavr satskyse
