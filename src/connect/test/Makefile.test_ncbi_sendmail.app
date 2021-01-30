# $Id$

APP = test_ncbi_sendmail
SRC = test_ncbi_sendmail
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
LINK = $(C_LINK)

CHECK_CMD =

WATCHERS = lavr
