# $Id$

APP = test_ncbi_http_get
SRC = test_ncbi_http_get
LIB = connssl connect $(NCBIATOMIC_LIB)

CPPFLAGS = $(TLS_INCLUDE) $(ORIG_CPPFLAGS)
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
LINK = $(C_LINK)
#LINK = purify $(C_LINK)

CHECK_CMD = test_ncbi_http_get.sh /CHECK_NAME=test_ncbi_http_get
CHECK_COPY = test_ncbi_http_get.sh ../../check/ncbi_test_data

WATCHERS = lavr
