# $Id$

APP = blobrwd
SRC = blobrwd

CPPFLAGS = $(ORIG_CPPFLAGS) $(CMPRS_INCLUDE)

LIB  = dbapi_util_blobstore dbapi_driver xcompress $(CMPRS_LIB) xutil xncbi
LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)


WATCHERS = ucko
