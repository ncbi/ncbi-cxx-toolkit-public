# $Id$

APP = psg_client
SRC = psg_client_app processing performance
LIB = psg_client id2 seqsplit seqset $(SEQ_LIBS) pub medline biblio general xser xconnserv \
	  xxconnect2 xconnect $(COMPRESS_LIBS) xutil xncbi

LIBS = $(PSG_CLIENT_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

WATCHERS = sadyrovr
CHECK_REQUIRES = in-house-resources
CHECK_CMD = psg_client test
CHECK_COPY = psg_client_test.json
