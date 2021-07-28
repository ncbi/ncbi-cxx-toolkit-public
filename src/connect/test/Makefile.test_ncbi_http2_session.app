# $Id$

APP = test_ncbi_http2_session
SRC = test_ncbi_http2_session
LIB = xxconnect2 xconnect xncbi

LIBS = $(XXCONNECT2_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = MT

CHECK_CMD =

WATCHERS = sadyrovr
