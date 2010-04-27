# $Id$

APP = test_ncbi_conn
SRC = test_ncbi_conn
LIB = xconnect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_conn.sh
CHECK_COPY = test_ncbi_conn.sh

WATCHERS = lavr
