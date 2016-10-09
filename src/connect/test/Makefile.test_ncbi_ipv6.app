# $Id$

APP = test_ncbi_ipv6
SRC = test_ncbi_ipv6
LIB = connect

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD =

WATCHERS = lavr
