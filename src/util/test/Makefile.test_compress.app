#################################
# $Id$

APP = test_compress
SRC = test_compress
LIB = xutil xcompress $(CMPRS_LIB) xncbi
LIBS = $(CMPRS_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(ORIG_CPPFLAGS) $(CMPRS_INCLUDE)

CHECK_CMD =

