#################################
# $Id$

APP = test_compress_mt
SRC = test_compress_mt
LIB = xutil xcompress $(CMPRS_LIB) test_mt xncbi
LIBS = $(CMPRS_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(ORIG_CPPFLAGS) $(CMPRS_INCLUDE)

CHECK_CMD =

