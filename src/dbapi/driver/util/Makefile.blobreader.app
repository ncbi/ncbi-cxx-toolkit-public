# $Id$

APP = blobreader
SRC = blobreader blobstore

CPPFLAGS = $(ORIG_CPPFLAGS) $(CMPRS_INCLUDE)

LIB  = dbapi_driver xncbi xutil xcompress $(CMPRS_LIB)
LIBS = $(CMPRS_LIBS) $(ORIG_LIBS)

