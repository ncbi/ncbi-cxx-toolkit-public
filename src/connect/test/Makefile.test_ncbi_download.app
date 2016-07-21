# $Id$

APP = test_ncbi_download
SRC = test_ncbi_download
LIB = connect connssl $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_REQUIRES = GNUTLS

WATCHERS = lavr
