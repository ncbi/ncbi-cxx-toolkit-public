# $Id$

APP = test_ncbi_ipv6
SRC = test_ncbi_ipv6
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_ipv6 2607:F220:041E:4000::/52 /CHECK_NAME=test_ncbi_ipv6

WATCHERS = lavr
