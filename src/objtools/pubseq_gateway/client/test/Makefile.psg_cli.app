# $Id$

APP = psg_cli
SRC = psg_cli
LIB = psg_client psg_rpc psg_diag $(SEQ_LIBS) pub medline biblio general xser xconnserv xconnect xutil xncbi

LIBS = $(PSG_RPC_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(PSG_RPC_INCLUDE) $(ORIG_CPPFLAGS)

WATCHERS = sadyrovr dmitrie1
