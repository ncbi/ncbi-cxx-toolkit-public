#################################
# $Id$

APP = test_compress_archive
SRC = test_compress_archive
LIB = xcompress xutil xncbi
LIBS = $(ORIG_LIBS)
CPPFLAGS = $(ORIG_CPPFLAGS)

CHECK_CMD = test_compress_archive test all  /CHECK_NAME=test_compress_archive

WATCHERS = ivanov
