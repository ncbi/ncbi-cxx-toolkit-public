# $Id$

APP = blobwriter
SRC = blobwriter blobstore

LIB  = dbapi_driver xncbi xutil xcompress z bz2
LIBS = $(DL_LIBS) $(ORIG_LIBS)

