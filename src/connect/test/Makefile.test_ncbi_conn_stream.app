# $Id$

APP = test_ncbi_conn_stream
SRC = test_ncbi_conn_stream
LIB = xconnect xncbi xutil

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify -best-effort CC

CHECK_CMD =
