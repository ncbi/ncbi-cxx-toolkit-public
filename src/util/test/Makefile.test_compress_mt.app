#################################
# $Id$

APP = test_compress_mt
SRC = test_compress_mt
LIB = xcompress xutil $(CMPRS_LIB) test_mt xncbi
LIBS = $(CMPRS_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(ORIG_CPPFLAGS) $(CMPRS_INCLUDE)

CHECK_CMD = test_compress_mt z
CHECK_CMD = test_compress_mt bz2
CHECK_CMD = test_compress_mt lzo
CHECK_TIMEOUT = 250

WATCHERS = ivanov
