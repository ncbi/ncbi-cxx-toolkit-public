#################################
# $Id$

APP = test_compress
SRC = test_compress
LIB = xcompress xutil $(CMPRS_LIB) xncbi
LIBS = $(CMPRS_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(ORIG_CPPFLAGS) $(CMPRS_INCLUDE)

CHECK_CMD = test_compress z     /CHECK_NAME=test_compress_mt_z
CHECK_CMD = test_compress zstd  /CHECK_NAME=test_compress_mt_zstd
CHECK_CMD = test_compress bz2   /CHECK_NAME=test_compress_mt_bz2
CHECK_CMD = test_compress lzo   /CHECK_NAME=test_compress_mt_lzo

WATCHERS = ivanov
