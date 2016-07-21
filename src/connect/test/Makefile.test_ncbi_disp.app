# $Id$

APP = test_ncbi_disp
SRC = test_ncbi_disp
LIB = connect connssl $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD =
CHECK_TIMEOUT = 30
CHECK_REQUIRES = GNUTLS

WATCHERS = lavr
