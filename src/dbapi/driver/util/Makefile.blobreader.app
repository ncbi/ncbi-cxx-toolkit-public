# $Id$

APP = blobreader
SRC = blobreader blobstore

LIB  = dbapi_driver xncbi xutil xcompress $(CMPRS_LIB)
LIBS = $(CMPRS_LIBS) $(ORIG_LIBS)

