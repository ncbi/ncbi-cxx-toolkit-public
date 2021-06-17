# $Id$

APP = test_fw
SRC = test_fw
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
LINK = $(C_LINK)

WATCHERS = lavr
