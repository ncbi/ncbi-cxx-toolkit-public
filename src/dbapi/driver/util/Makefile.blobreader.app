# $Id$

APP = blobreader
SRC = blobreader blobstore

CPPFLAGS = $(ORIG_CPPFLAGS) $(CMPRS_INCLUDE)

LIB  = dbapi_driver xcompress $(CMPRS_LIB) xutil xncbi
LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

