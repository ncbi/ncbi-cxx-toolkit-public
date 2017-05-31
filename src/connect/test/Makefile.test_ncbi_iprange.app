# $Id$

APP = test_ncbi_iprange
SRC = test_ncbi_iprange
LIB = connect

LIBS = $(NETWORK_LIBS) $(C_LIBS)
LINK = $(C_LINK)
# LINK = purify $(C_LINK)

WATCHERS = lavr
