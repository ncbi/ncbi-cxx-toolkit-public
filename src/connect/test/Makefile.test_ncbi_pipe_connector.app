# $Id$

APP = test_ncbi_pipe_connector
SRC = test_ncbi_pipe_connector ncbi_conntest
LIB = xconnect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD =

WATCHERS = lavr
