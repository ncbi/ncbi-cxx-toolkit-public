#################################
# $Id$
# Author:  Liangshou Wu
#################################


###  BASIC PROJECT SETTINGS
APP = fetch_gbproject
SRC = fetch_gbproject
# OBJ =

CPPFLAGS = $(ORIG_CPPFLAGS)

LIB_s = uudutil trackmgr gbproj submit seqset $(SEQ_LIBS) pub medline biblio general \
      xser xconnserv $(COMPRESS_LIBS) xutil xconnect xncbi

LIB = $(LIB_s:%=%$(STATIC))

LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

