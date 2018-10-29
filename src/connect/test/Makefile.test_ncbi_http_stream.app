# $Id$

APP = test_ncbi_http_stream
SRC = test_ncbi_http_stream
LIB = xconnect xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_ncbi_http_stream https://www.ncbi.nlm.nih.gov/Service/index.html / /Service/index.html

WATCHERS = lavr
