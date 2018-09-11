# $Id$

APP = psg_cli
SRC = psg_cli
LIB = psg_client $(SEQ_LIBS) pub medline biblio general xser xconnserv xconnect xutil xncbi

LIBS = $(PSG_CLIENT_LIBS) $(ORIG_LIBS)

WATCHERS = sadyrovr dmitrie1
