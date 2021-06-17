#################################
# $Id$

APP = test_compress
SRC = test_compress
LIB = xcompress xutil $(CMPRS_LIB) xncbi
LIBS = $(CMPRS_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(ORIG_CPPFLAGS) $(CMPRS_INCLUDE)

CHECK_CMD = test_compress z
CHECK_CMD = test_compress bz2
CHECK_CMD = test_compress lzo

WATCHERS = ivanov
