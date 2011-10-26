# $Id$

APP = test_ncbi_ftp_download
SRC = test_ncbi_ftp_download
CPPFLAGS = $(CMPRS_INCLUDE) $(ORIG_CPPFLAGS)
LIB = xconnect $(COMPRESS_LIBS) xutil xncbi

LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD =

WATCHERS = lavr
