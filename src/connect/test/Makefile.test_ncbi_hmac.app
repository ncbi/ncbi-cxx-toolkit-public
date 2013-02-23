# $Id$

APP = test_ncbi_hmac
SRC = test_ncbi_hmac
LIB = xutil xconnect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD =

WATCHERS = lavr
