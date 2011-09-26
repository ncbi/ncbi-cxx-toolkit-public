# $Id$

APP = test_ncbi_rate_monitor
SRC = test_ncbi_rate_monitor
LIB = xconnect xncbi

LIBS = $(NETWORK_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD =

WATCHERS = lavr
