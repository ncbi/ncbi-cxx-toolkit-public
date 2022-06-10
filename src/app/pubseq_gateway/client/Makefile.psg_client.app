# $Id$

APP = psg_client
SRC = psg_client_app processing
LIB = psg_client id2 seqsplit seqset $(SEQ_LIBS) pub medline biblio general xser xconnserv \
      xxconnect2 xconnect $(COMPRESS_LIBS) xutil xncbi

LIBS = $(PSG_CLIENT_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

CHECK_CMD = python3 psg_client.py test /CHECK_NAME=psg_client.py
CHECK_COPY = psg_client.py
CHECK_REQUIRES = in-house-resources -Valgrind

WATCHERS = sadyrovr
