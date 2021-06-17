# $Id$

APP = test_ncbi_blowfish
SRC = test_ncbi_blowfish
LIB = xconnect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD =

WATCHERS = lavr
