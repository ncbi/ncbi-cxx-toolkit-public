# $Id$

APP = test_ncbi_conn_stream
OBJ = test_ncbi_conn_stream
LIB = xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify -best-effort CC

CHECK_CMD =
