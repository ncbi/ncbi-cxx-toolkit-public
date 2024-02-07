# $Id$

APP = test_ncbi_pipe
SRC = test_ncbi_pipe
LIB = xconnect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD =

# Make sure the test directory is non-empty
CHECK_COPY = Makefile.test_ncbi_pipe.app

WATCHERS = ivanov lavr
