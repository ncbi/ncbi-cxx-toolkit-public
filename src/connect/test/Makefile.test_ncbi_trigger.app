# $Id$

APP = test_ncbi_trigger
SRC = test_ncbi_trigger
LIB = xconnect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

REQUIRES = MT

CHECK_CMD = test_ncbi_trigger.sh
CHECK_COPY = test_ncbi_trigger.sh
