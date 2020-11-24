# $Id$

APP = test_ncbi_iprange
SRC = test_ncbi_iprange
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(C_LIBS)
LINK = $(C_LINK)
# LINK = purify $(C_LINK)

CHECK_CMD = test_ncbi_iprange 127.0.0.1 2607:F220:041E:4000::/52 130.14.29.128-255 /CHECK_NAME=test_ncbi_iprange

WATCHERS = lavr
