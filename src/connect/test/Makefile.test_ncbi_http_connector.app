# $Id$

APP = test_ncbi_http_connector
SRC = test_ncbi_http_connector
LIB = xconntest connect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD  =
