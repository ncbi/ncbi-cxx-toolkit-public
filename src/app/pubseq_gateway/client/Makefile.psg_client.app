# $Id$

APP = psg_client
SRC = psg_client_app processing performance
LIB = psg_client seqset $(SEQ_LIBS) pub medline biblio general xser xconnserv xconnect xutil xncbi

LIBS = $(PSG_CLIENT_LIBS) $(ORIG_LIBS)

WATCHERS = sadyrovr
CHECK_CMD = 'cat testing.json |psg_client testing' /CHECK_NAME=test_psg_client
CHECK_COPY = testing.json
