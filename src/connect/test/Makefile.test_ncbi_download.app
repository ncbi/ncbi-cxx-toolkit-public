# $Id$

APP = test_ncbi_download
SRC = test_ncbi_download
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

WATCHERS = lar
