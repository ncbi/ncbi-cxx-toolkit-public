# $Id$

APP = test_fstream_pushback
SRC = test_fstream_pushback
LIB = xpbacktest xutil xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = ./test_fstream_pushback.sh
