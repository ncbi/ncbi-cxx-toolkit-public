#################################
# $Id$
# Author:  Liangshou Wu
#################################


###  BASIC PROJECT SETTINGS
APP = fetch_gbproject
SRC = fetch_gbproject
# OBJ =

CPPFLAGS = $(ORIG_CPPFLAGS)

LIB_s = uudutil gbproj submit seqset $(SEQ_LIBS) pub medline biblio general \
      xser xconnserv xutil xconnect $(COMPRESS_LIBS) xncbi

LIB = $(LIB_s:%=%$(STATIC))

LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

