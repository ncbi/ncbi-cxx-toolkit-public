# $Id$

APP = test_ncbi_pipe
SRC = test_ncbi_pipe
LIB = xconnect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD =

# Make sure the test directory is non-empty
CHECK_COPY = test_ncbi_pipe.cpp

WATCHERS = ivanov lavr
