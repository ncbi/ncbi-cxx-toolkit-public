# $Id$

APP = test_ncbi_pipe
SRC = test_ncbi_pipe
LIB = xconnect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD =

WATCHERS = ivanov lavr
