#################################
# $Id$
# Author:  Aleksey Grichenko (grichenk@ncbi.nlm.nih.gov)
#################################

# Build serialization test application "test_serial_io"
#################################

APP = test_serial_io
SRC = test_serial_io

REQUIRES = MT

LIB = xconnect xser xutil xncbi $(COMPRESS_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD  = test_serial_io
CHECK_TIMEOUT = 5000

WATCHERS = grichenk
