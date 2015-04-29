# $Id$

SRC = file_upload
APP = file_upload.cgi

LIB = xconnserv xcgi xthrserv xconnect xutil test_boost xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = sadyrovr
