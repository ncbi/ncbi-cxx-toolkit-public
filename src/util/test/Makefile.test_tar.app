# $Id$

APP = test_tar
SRC = test_tar
LIB = tar xcompress $(CMPRS_LIB) xncbi
LIBS = $(CMPRS_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(ORIG_CPPFLAGS) $(CMPRS_INCLUDE)
