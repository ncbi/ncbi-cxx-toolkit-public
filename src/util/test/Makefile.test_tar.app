# $Id$

APP = test_tar
SRC = test_tar
CPPFLAGS = $(CMPRS_INCLUDE) $(ORIG_CPPFLAGS)
LIB = $(COMPRESS_LIBS) xutil xncbi

LIBS = $(CMPRS_LIBS) $(ORIG_LIBS)

CHECK_REQUIRES = unix

CHECK_CMD = test_tar.sh
CHECK_COPY = test_tar.sh
CHECK_TIMEOUT = 600

WATCHERS = lavr
