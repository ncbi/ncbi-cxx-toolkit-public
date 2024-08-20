# $Id$

APP = psg_client_sample
SRC = psg_client_sample
LIB = psg_client id2 seqsplit seqset $(SEQ_LIBS) pub medline biblio general xser xconnserv \
      xxconnect2 xconnect $(COMPRESS_LIBS) xutil xncbi

LIBS = $(PSG_CLIENT_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

WATCHERS = sadyrovr
