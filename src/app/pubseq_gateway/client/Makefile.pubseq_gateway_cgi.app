# $Id$

APP = pubseq_gateway.cgi
SRC = psg_client_cgi processing
LIB = psg_client id2 seqsplit seqset $(SEQ_LIBS) pub medline biblio general xser xconnserv \
	  xxconnect2 xconnect $(COMPRESS_LIBS) xcgi xutil xncbi

LIBS = $(PSG_CLIENT_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

WATCHERS = sadyrovr
